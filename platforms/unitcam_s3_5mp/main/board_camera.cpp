#include "board_camera.hpp"
#include "board_pins.hpp"
#include <esp_camera.h>
#include <esp_log.h>

static const char* TAG = "board_cam";

bool board_camera_init()
{
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAMERA_PIN_D0;
    config.pin_d1 = CAMERA_PIN_D1;
    config.pin_d2 = CAMERA_PIN_D2;
    config.pin_d3 = CAMERA_PIN_D3;
    config.pin_d4 = CAMERA_PIN_D4;
    config.pin_d5 = CAMERA_PIN_D5;
    config.pin_d6 = CAMERA_PIN_D6;
    config.pin_d7 = CAMERA_PIN_D7;
    config.pin_xclk = CAMERA_PIN_XCLK;
    config.pin_pclk = CAMERA_PIN_PCLK;
    config.pin_vsync = CAMERA_PIN_VSYNC;
    config.pin_href = CAMERA_PIN_HREF;
    config.pin_sscb_sda = CAMERA_PIN_SIOD;
    config.pin_sscb_scl = CAMERA_PIN_SIOC;
    config.pin_pwdn = CAMERA_PIN_PWDN;
    config.pin_reset = CAMERA_PIN_RESET;
    config.xclk_freq_hz = BOARD_XCLK_FREQ_HZ;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "camera ok");
    return true;
}

bool board_camera_grab_jpeg(std::vector<uint8_t>& out)
{
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb == nullptr)
    {
        ESP_LOGW(TAG, "fb null");
        return false;
    }
    if (fb->format != PIXFORMAT_JPEG)
    {
        ESP_LOGW(TAG, "not jpeg");
        esp_camera_fb_return(fb);
        return false;
    }
    out.assign(fb->buf, fb->buf + fb->len);
    esp_camera_fb_return(fb);
    return true;
}
