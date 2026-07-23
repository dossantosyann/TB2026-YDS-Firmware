/**
 * @file sdcard.h
 * @brief microSD card over SPI3 (VSPI), mounted as a FATFS volume at "/sdcard".
 *
 * The driver owns SPI3 entirely: sdcard_mount() initializes the bus, attaches the
 * card via sdspi, and mounts the FAT volume; after that, read/write files with the
 * standard POSIX API on paths under "/sdcard" (e.g. fopen("/sdcard/track.mp3", "rb")).
 *
 * Like the GPIO expander, this driver owns no interrupt and no event queue: card
 * presence is read synchronously via sdcard_present(). Hot-plug handling (an ISR on
 * PIN_SD_DETECT that mounts/unmounts) belongs to the consumer, not here.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @defgroup drivers_sdcard microSD card (FATFS over SPI)
 * @ingroup drivers
 * @brief Mount/unmount a FAT volume at "/sdcard" and read card presence.
 * @{
 */

/** @brief Mount point; prefix every path passed to fopen/fread/fwrite. */
#define SDCARD_MOUNT_POINT "/sdcard"

/**
 * @brief Initialize SPI3, attach the card via sdspi, and mount the FAT volume.
 *
 * Does not auto-format: if the card has no valid FAT, mounting fails and the card
 * is left untouched. After this returns ESP_OK, files are accessible with the POSIX
 * API under @ref SDCARD_MOUNT_POINT.
 *
 * @return ESP_OK on success; ESP_ERR_NOT_FOUND if no card is present;
 *         or the underlying SPI/sdspi/FATFS error.
 */
esp_err_t sdcard_mount(void);

/**
 * @brief Unmount the FAT volume and detach the sdspi device.
 *
 * Flushes FATFS. The SPI3 bus itself stays initialized: on the classic ESP32 both
 * SPI hosts share one DMA clock bit, so freeing SPI3 would stall an in-flight
 * display (SPI2) DMA transfer (see sdcard_mount()).
 *
 * @return ESP_OK on success, or the underlying error.
 */
esp_err_t sdcard_unmount(void);

/**
 * @brief Report whether a card is physically inserted (synchronous read of PIN_SD_DETECT).
 *
 * Configures the detect GPIO on first call. Safe to call before mounting, e.g. to
 * decide whether sdcard_mount() is worth attempting.
 *
 * @return true if a card is inserted, false otherwise.
 */
bool sdcard_present(void);

/**
 * @brief Actual SPI clock negotiated with the card by the host, in kHz.
 *
 * May be lower than the frequency requested at mount (the host rounds down to
 * a reachable divider), so this is the value to trust when checking throughput.
 *
 * @return Frequency in kHz while mounted, 0 otherwise.
 */
int sdcard_freq_khz(void);

/** @} */
