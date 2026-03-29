#include "simple_picsaver.hpp"
#include "board_camera.hpp"
#include "board_gpio.hpp"
#include "board_sd.hpp"
#include "pic_crypto.hpp"
#include <cstdio>
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

    // Elapsed since boot (esp_timer): HHHH hours, MM minutes within that hour, SS seconds — unique at 1 fps.
    const uint64_t total_sec = esp_timer_get_time() / 1000000ULL;
    const uint64_t h = total_sec / 3600ULL;
    const uint64_t m = (total_sec % 3600ULL) / 60ULL;
    const uint64_t s = total_sec % 60ULL;

    char path[128];
    if (pic_crypto_is_enabled())
        std::snprintf(path, sizeof(path), "%s/%04llu_%02llu_%02llu.ucam", k_folder, h, m, s);
    else
        std::snprintf(path, sizeof(path), "%s/%04llu_%02llu_%02llu.jpg", k_folder, h, m, s);

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
