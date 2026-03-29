#include <Arduino.h>
#include "board_camera.hpp"
#include "board_gpio.hpp"
#include "board_thermal.hpp"
#include "simple_picsaver.hpp"
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "app";

extern "C" void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    initArduino();

    board_gpio_init();
    board_led_set(true);

    board_thermal_init();
    board_thermal_start_monitor_task();

    if (!board_camera_init())
    {
        ESP_LOGE(TAG, "camera failed — halt");
        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    if (!simple_picsaver_init())
    {
        ESP_LOGE(TAG, "SD not available — camera runs but no saves");
        board_led_set(false);
    }

    ESP_LOGI(TAG, "loop: 1 capture / second");

    while (true)
    {
        if (!board_thermal_shutdown_requested())
            simple_picsaver_tick();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
