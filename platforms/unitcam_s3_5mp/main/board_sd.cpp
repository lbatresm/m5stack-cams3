#include "board_sd.hpp"
#include "board_pins.hpp"
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <driver/gpio.h>
#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <esp_vfs_fat.h>
#include <esp_log.h>

static const char* TAG = "board_sd";
static const char* MOUNT_POINT = "/sdcard";

static sdmmc_card_t* s_card = nullptr;
static bool s_mounted = false;
static bool s_spi_bus_inited = false;
static spi_host_device_t s_spi_host = SPI3_HOST;

bool board_sd_mount()
{
    if (s_mounted)
        return true;

    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 10000;

    const spi_host_device_t spi_host = static_cast<spi_host_device_t>(host.slot);

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = BOARD_PIN_SD_MOSI;
    bus_cfg.miso_io_num = BOARD_PIN_SD_MISO;
    bus_cfg.sclk_io_num = BOARD_PIN_SD_CLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.data4_io_num = -1;
    bus_cfg.data5_io_num = -1;
    bus_cfg.data6_io_num = -1;
    bus_cfg.data7_io_num = -1;
    bus_cfg.max_transfer_sz = 8192;
    bus_cfg.flags = 0;
    bus_cfg.isr_cpu_id = INTR_CPU_ID_AUTO;
    bus_cfg.intr_flags = 0;

    esp_err_t ret = spi_bus_initialize(spi_host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "spi_bus_initialize: %s", esp_err_to_name(ret));
        return false;
    }

    s_spi_host = spi_host;
    s_spi_bus_inited = true;

    const uint8_t pins[] = {BOARD_PIN_SD_CLK, BOARD_PIN_SD_MISO, BOARD_PIN_SD_MOSI};
    for (uint8_t p : pins)
        gpio_set_drive_capability(static_cast<gpio_num_t>(p), GPIO_DRIVE_CAP_3);

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = static_cast<gpio_num_t>(BOARD_PIN_SD_CS);
    slot_config.host_id = spi_host;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);

    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "no SD card or mount failed: %s (insert card and reboot, or wait for retry)", esp_err_to_name(ret));
        spi_bus_free(s_spi_host);
        s_spi_bus_inited = false;
        s_card = nullptr;
        return false;
    }

    s_mounted = true;

    uint64_t total = 0;
    uint64_t free_b = 0;
    if (esp_vfs_fat_info(MOUNT_POINT, &total, &free_b) == ESP_OK)
    {
        ESP_LOGI(TAG, "SD mounted, total ~%llu MB, free ~%llu MB",
                 static_cast<unsigned long long>(total / (1024 * 1024)),
                 static_cast<unsigned long long>(free_b / (1024 * 1024)));
    }
    else
    {
        ESP_LOGI(TAG, "SD mounted");
    }
    return true;
}

void board_sd_unmount()
{
    if (!s_mounted)
        return;

    esp_err_t err = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
    if (err != ESP_OK)
        ESP_LOGW(TAG, "unmount: %s", esp_err_to_name(err));

    s_card = nullptr;
    s_mounted = false;

    if (s_spi_bus_inited)
    {
        spi_bus_free(s_spi_host);
        s_spi_bus_inited = false;
    }
}

bool board_sd_ok()
{
    return s_mounted;
}

bool board_sd_mkdir(const char* path)
{
    if (!s_mounted || path == nullptr)
        return false;

    struct stat st;
    if (stat(path, &st) == 0)
        return S_ISDIR(st.st_mode);

    if (mkdir(path, 0755) != 0)
    {
        ESP_LOGE(TAG, "mkdir %s: errno %d", path, errno);
        return false;
    }
    return true;
}

bool board_sd_write_file(const char* path, const uint8_t* data, size_t len)
{
    if (!s_mounted || path == nullptr || data == nullptr || len == 0)
        return false;

    FILE* f = std::fopen(path, "wb");
    if (f == nullptr)
    {
        ESP_LOGE(TAG, "fopen %s failed", path);
        return false;
    }

    const size_t w = std::fwrite(data, 1, len, f);
    if (w != len)
    {
        std::fclose(f);
        ESP_LOGE(TAG, "short write %u / %u", static_cast<unsigned>(w), static_cast<unsigned>(len));
        return false;
    }

    // Flush FAT/cache and physical card so the volume stays consistent if power is cut or the card is removed.
    if (std::fflush(f) != 0 || fsync(fileno(f)) != 0)
    {
        const int e = errno;
        std::fclose(f);
        ESP_LOGE(TAG, "sync failed for %s (errno %d)", path, e);
        return false;
    }

    std::fclose(f);
    return true;
}
