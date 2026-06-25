#include "sdcard.h"
#include "spi_bus.h"
#include "board_pins.h"

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "sdcard";

/* FATFS mount tuning. 5 concurrent open files is plenty for an MP3 player (one
   track + a couple of metadata reads); 16 KiB allocation unit balances throughput
   against slack on small files. */
#define SD_MAX_OPEN_FILES   5
#define SD_ALLOC_UNIT_SIZE  (16 * 1024)

static sdmmc_card_t *s_card;       /* non-NULL while mounted */
static bool          s_detect_cfg; /* PIN_SD_DETECT configured once, lazily */

/* Card-detect switch shorts to GND when a card is seated, so with an internal
   pull-up the pin reads LOW = present. NOTE: verify against the socket footprint;
   if the detect line is active-high, flip this. */
#define SD_DETECT_PRESENT_LEVEL 0

esp_err_t sdcard_mount(void)
{
    if (s_card != NULL) {
        return ESP_OK;  /* already mounted */
    }
    if (!sdcard_present()) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = spi_bus_sdcard_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI3 init failed: %s", esp_err_to_name(err));
        return err;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI_BUS_SDCARD_HOST;

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = PIN_SD_CS;
    slot.host_id = SPI_BUS_SDCARD_HOST;

    const esp_vfs_fat_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = SD_MAX_OPEN_FILES,
        .allocation_unit_size = SD_ALLOC_UNIT_SIZE,
    };

    err = esp_vfs_fat_sdspi_mount(SDCARD_MOUNT_POINT, &host, &slot, &mount_cfg, &s_card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount failed: %s", esp_err_to_name(err));
        spi_bus_free(SPI_BUS_SDCARD_HOST);
        s_card = NULL;
        return err;
    }

    ESP_LOGI(TAG, "mounted at %s", SDCARD_MOUNT_POINT);
    return ESP_OK;
}

esp_err_t sdcard_unmount(void)
{
    if (s_card == NULL) {
        return ESP_OK;
    }
    esp_err_t err = esp_vfs_fat_sdcard_unmount(SDCARD_MOUNT_POINT, s_card);
    s_card = NULL;
    spi_bus_free(SPI_BUS_SDCARD_HOST);
    return err;
}

bool sdcard_present(void)
{
    if (!s_detect_cfg) {
        const gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << PIN_SD_DETECT,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&cfg);
        s_detect_cfg = true;
    }
    return gpio_get_level(PIN_SD_DETECT) == SD_DETECT_PRESENT_LEVEL;
}
