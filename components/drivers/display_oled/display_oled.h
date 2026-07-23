/**
 * @file display_oled.h
 * @brief SSD1333 OLED driver (NHD-1.91-176176UBC3, 176x176, 4-wire SPI).
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
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

/** Highest brightness level accepted by display_oled_set_brightness(). */
#define DISPLAY_OLED_BRIGHTNESS_MAX 15

/**
 * @brief Set the global panel brightness (SSD1333 Master Current, cmd 0xC7).
 *
 * Scales the segment output current uniformly across all colours, so it dims the
 * whole picture without shifting the colour balance -- and lowers the panel's
 * current draw, which matters on battery. The init sequence leaves this at the
 * maximum; call this to override it.
 *
 * Runs on the caller's task; call from the UI task only (same task as
 * display_oled_draw()), never concurrently with a blit.
 *
 * @param level  0 (dimmest) .. DISPLAY_OLED_BRIGHTNESS_MAX (brightest); clamped.
 * @return ESP_OK on success, or the SPI error.
 */
esp_err_t display_oled_set_brightness(uint8_t level);

/**
 * @brief Put the panel to sleep (SSD1333 Display OFF, cmd 0xAE).
 *
 * Blanks the output and drops the panel to its ~2 µA sleep current; the RAM contents
 * and all configuration are retained, so display_oled_wake() restores the last frame
 * with no re-init and no redraw. Building block for a future screen auto-off / sleep
 * policy -- this driver applies no timeout of its own.
 *
 * Not serialised internally: call it from the same task as display_oled_draw() (the UI
 * task), never concurrently with a blit.
 *
 * @return ESP_OK on success, or the SPI error.
 */
esp_err_t display_oled_sleep(void);

/**
 * @brief Wake the panel from sleep (SSD1333 Display ON, cmd 0xAF).
 *
 * Undoes display_oled_sleep(); the previously shown frame reappears. Same task
 * constraint as display_oled_sleep().
 *
 * @return ESP_OK on success, or the SPI error.
 */
esp_err_t display_oled_wake(void);

/** @} */
