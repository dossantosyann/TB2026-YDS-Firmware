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
static bool          s_bus_up;     /* SPI3 bus initialized once, never freed */
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

    /* Bring the SPI3 bus up once and keep it for the lifetime of the firmware. Never
       spi_bus_free() it: on the classic ESP32 both SPI hosts share one DMA clock bit
       (DPORT_SPI_DMA_CLK_EN), and IDF ref-counts SPI2/SPI3 DMA separately — freeing
       SPI3 gates the clock and stalls an in-flight display (SPI2) DMA transfer, which
       hangs the UI task inside its timeout-less polling transmit. */
    if (!s_bus_up) {
        esp_err_t bus_err = spi_bus_sdcard_init();
        if (bus_err != ESP_OK) {
            ESP_LOGE(TAG, "SPI3 init failed: %s", esp_err_to_name(bus_err));
            return bus_err;
        }
        s_bus_up = true;
    }
    esp_err_t err;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI_BUS_SDCARD_HOST;
    /* 25 MHz: SD SPI-mode spec ceiling. 40 MHz caused data-CRC failures under
       sustained load (24-bit WAV streaming) — out of spec, no timing margin.
       25 MHz still yields ~3 MB/s, ample for the ~1.15 MB/s worst case (192 kHz
       24-bit WAV) through FATFS. Drop to SDMMC_FREQ_DEFAULT if mounts fail. */
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

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
    /* Removes the sdspi device but keeps the SPI3 bus up (see sdcard_mount). */
    esp_err_t err = esp_vfs_fat_sdcard_unmount(SDCARD_MOUNT_POINT, s_card);
    s_card = NULL;
    return err;
}

int sdcard_freq_khz(void)
{
    return s_card ? s_card->real_freq_khz : 0;
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
