#include "simple_picsaver.hpp"
#include "board_camera.hpp"
#include "board_gpio.hpp"
#include "board_sd.hpp"
#include "pic_crypto.hpp"
#include <cstdio>
#include <ctime>
#include <vector>
#include <esp_log.h>
#include <esp_timer.h>

static const char* TAG = "picsaver";
// VFS mount point from board_sd (esp_vfs_fat_sdspi_mount)
static constexpr const char* k_folder = "/sdcard/simple_picsaver";
static bool s_ok = false;
static int s_no_sd_ticks = 0;

bool simple_picsaver_init()
{
    pic_crypto_init();

    if (!board_sd_mount())
        return false;

    if (!board_sd_mkdir(k_folder))
    {
        ESP_LOGE(TAG, "mkdir %s failed", k_folder);
        board_sd_unmount();
        return false;
    }

    s_ok = true;
    s_no_sd_ticks = 0;
    ESP_LOGI(TAG, "ready, saving to %s", k_folder);
    return true;
}

void simple_picsaver_tick()
{
    if (!s_ok || !board_sd_ok())
    {
        // Retry ~every 30 s so you can insert the card without rebooting
        if (++s_no_sd_ticks >= 30)
        {
            s_no_sd_ticks = 0;
            ESP_LOGI(TAG, "retry SD mount...");
            simple_picsaver_init();
        }
        return;
    }

    std::vector<uint8_t> jpeg;
    if (!board_camera_grab_jpeg(jpeg))
        return;

    const time_t t = time(nullptr);
    struct tm lt_buf {};
    struct tm* lt = localtime_r(&t, &lt_buf);

    char path[128];
    if (lt != nullptr && (lt->tm_year + 1900) >= 2024)
    {
        if (pic_crypto_is_enabled())
            std::snprintf(path, sizeof(path), "%s/%04d%02d%02d_%02d%02d%02d_%llu.ucam", k_folder, lt->tm_year + 1900,
                          lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec,
                          static_cast<unsigned long long>(esp_timer_get_time() / 1000ULL));
        else
            std::snprintf(path, sizeof(path), "%s/%04d%02d%02d_%02d%02d%02d_%llu.jpg", k_folder, lt->tm_year + 1900,
                          lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec,
                          static_cast<unsigned long long>(esp_timer_get_time() / 1000ULL));
    }
    else
    {
        const uint64_t us = esp_timer_get_time();
        if (pic_crypto_is_enabled())
            std::snprintf(path, sizeof(path), "%s/boot_%llu.ucam", k_folder,
                          static_cast<unsigned long long>(us / 1000000ULL));
        else
            std::snprintf(path, sizeof(path), "%s/boot_%llu.jpg", k_folder, static_cast<unsigned long long>(us / 1000000ULL));
    }

    const uint8_t* write_ptr = jpeg.data();
    size_t write_len = jpeg.size();
    std::vector<uint8_t> encrypted;
    if (pic_crypto_is_enabled())
    {
        if (!pic_crypto_encrypt_jpeg(jpeg.data(), jpeg.size(), encrypted))
        {
            ESP_LOGE(TAG, "encrypt failed");
            return;
        }
        write_ptr = encrypted.data();
        write_len = encrypted.size();
    }

    if (!board_sd_write_file(path, write_ptr, write_len))
        ESP_LOGE(TAG, "write failed: %s", path);
    else
    {
        ESP_LOGI(TAG, "saved %s (jpeg %u bytes)", path, static_cast<unsigned>(jpeg.size()));
        static bool led = false;
        led = !led;
        board_led_set(led);
    }
}
