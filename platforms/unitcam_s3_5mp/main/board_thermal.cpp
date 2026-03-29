#include "board_thermal.hpp"
#include "board_gpio.hpp"
#include "board_sd.hpp"
#include <driver/temperature_sensor.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "board_thermal";

// Internal die temperature (often higher than ambient). Adjust if you get false trips.
static constexpr float kCriticalCelsius = 90.0f;
static constexpr uint32_t kCheckIntervalMs = 2 * 60 * 1000;

static temperature_sensor_handle_t s_tsens = nullptr;
static volatile bool s_shutdown = false;

static esp_err_t read_chip_temp_celsius(float* out_celsius)
{
    if (s_tsens == nullptr || out_celsius == nullptr)
        return ESP_ERR_INVALID_STATE;

    esp_err_t err = temperature_sensor_enable(s_tsens);
    if (err != ESP_OK)
        return err;

    err = temperature_sensor_get_celsius(s_tsens, out_celsius);
    temperature_sensor_disable(s_tsens);
    return err;
}

bool board_thermal_init(void)
{
    // Range must lie inside one HW band (e.g. 20–100 °C). 10–100 crosses bands → ESP_ERR_INVALID_ARG.
    const temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);

    esp_err_t err = temperature_sensor_install(&cfg, &s_tsens);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "temperature_sensor_install failed: %s — thermal protection disabled", esp_err_to_name(err));
        s_tsens = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "chip temp monitor: every %u s, critical %.0f C (first reading right after boot)",
             (unsigned)(kCheckIntervalMs / 1000), kCriticalCelsius);
    return true;
}

bool board_thermal_shutdown_requested(void)
{
    return s_shutdown;
}

static void thermal_monitor_task(void* /*arg*/)
{
    if (s_tsens == nullptr)
    {
        vTaskDelete(nullptr);
        return;
    }

    const TickType_t interval = pdMS_TO_TICKS(kCheckIntervalMs);

    for (;;)
    {
        float celsius = 0.f;
        const esp_err_t err = read_chip_temp_celsius(&celsius);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "read failed: %s", esp_err_to_name(err));
            vTaskDelay(interval);
            continue;
        }

        ESP_LOGI(TAG, "chip temperature: %.1f C", static_cast<double>(celsius));

        if (celsius >= kCriticalCelsius)
        {
            ESP_LOGW(TAG, "CRITICAL %.1f C >= %.0f C — stopping firmware (reset to recover)",
                     static_cast<double>(celsius), static_cast<double>(kCriticalCelsius));
            s_shutdown = true;
            vTaskDelay(pdMS_TO_TICKS(2000));
            if (board_sd_ok())
                board_sd_unmount();
            board_led_set(false);
            while (true)
                vTaskDelay(pdMS_TO_TICKS(60000));
        }

        vTaskDelay(interval);
    }
}

void board_thermal_start_monitor_task(void)
{
    if (s_tsens == nullptr)
        return;

    const BaseType_t ok = xTaskCreate(thermal_monitor_task, "thermal", 3072, nullptr, 5, nullptr);
    if (ok != pdPASS)
        ESP_LOGW(TAG, "xTaskCreate(thermal) failed — thermal protection disabled");
}
