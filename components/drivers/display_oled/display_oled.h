/**
 * @file display_oled.h
 * @brief SSD1333 OLED driver (NHD-1.91-176176UBC3, 176x176, 4-wire SPI).
 *
 * @defgroup drivers Drivers
 * @brief Chip-level drivers built on top of the bsp buses.
 */
#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"

/**
 * @defgroup drivers_display_oled OLED display (SSD1333)
 * @ingroup drivers
 * @brief Panel init and full-frame RGB565 blit over SPI.
 * @{
 */

/**
 * @brief Reset the panel and run the SSD1333 init sequence (gamma, contrast, display on).
 *
 * Configures the D/C# and RST GPIOs, pulses reset, then sends the datasheet init
 * sequence. Must be called once before any draw.
 *
 * @param spi  SPI device handle for the panel (from spi_bus_display_init()).
 * @return ESP_OK on success, or the GPIO/SPI error.
 */
esp_err_t display_oled_init(spi_device_handle_t spi);

/**
 * @brief Blit a full framebuffer to the panel.
 *
 * Streams the whole 176x176 RAM window, byte-swapping each native RGB565 pixel to the
 * panel's MSB-first order.
 *
 * @param buf  Framebuffer, native-order RGB565, GFX_W*GFX_H pixels.
 * @param len  Byte length; must equal 176*176*2.
 * @return ESP_OK on success, ESP_ERR_INVALID_SIZE if @p len is wrong, or the SPI error.
 */
esp_err_t display_oled_draw(const uint8_t *buf, size_t len);

/**
 * @brief Clear the panel to black directly, without a framebuffer.
 *
 * @return ESP_OK on success, or the SPI error.
 */
esp_err_t display_oled_clear(void);

/** @} */
