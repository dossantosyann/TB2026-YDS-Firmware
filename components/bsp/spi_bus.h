/**
 * @file spi_bus.h
 * @brief SPI buses: display on SPI2 (HSPI), SD card on SPI3 (VSPI).
 *
 * Both run on native IOMUX pins.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"

/**
 * @defgroup bsp_spi SPI buses
 * @ingroup bsp
 * @brief Display on SPI2 (HSPI), SD card on SPI3 (VSPI); both on native IOMUX pins.
 * @{
 */

#define SPI_BUS_DISPLAY_HOST SPI2_HOST  /**< Display host (HSPI). */
#define SPI_BUS_SDCARD_HOST  SPI3_HOST  /**< SD card host (VSPI). */

/**
 * @brief Initialize SPI2 and add the display as a device.
 *
 * @param[out] out_handle  Receives the display device handle.
 * @return ESP_OK on success, or the underlying driver error.
 */
esp_err_t spi_bus_display_init(spi_device_handle_t *out_handle);

/**
 * @brief Initialize SPI3 only; the sdcard driver attaches itself via sdspi
 *        (host = SPI_BUS_SDCARD_HOST, CS = PIN_SD_CS).
 *
 * @return ESP_OK on success, or the underlying driver error.
 */
esp_err_t spi_bus_sdcard_init(void);

/** @} */
