/**
 * @file gpio_expander.h
 * @brief PI4IOE5V9554A 8-bit I2C GPIO expander (PCA9554A-compatible) at 0x38.
 *
 * Channels are the EXP_* bit positions in board_pins.h. IO0/IO1 are outputs
 * (DAC mute, amp shutdown), IO2..IO7 are inputs (buttons, charger INOKB).
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup drivers_gpio_expander GPIO expander (PI4IOE5V9554A)
 * @ingroup drivers
 * @brief I2C access to the 8 expander channels: per-pin output, input read.
 * @{
 */

/**
 * @brief Attach the expander to the shared I2C bus and set pin directions.
 *
 * Drives all outputs low, then configures IO0/IO1 as outputs and IO2..IO7 as inputs.
 *
 * @param bus  Master bus handle from i2c_bus_init().
 * @return ESP_OK on success, or the I2C error (e.g. if the expander does not ACK).
 */
esp_err_t gpio_expander_init(i2c_master_bus_handle_t bus);

/**
 * @brief Drive one output channel (read-modify-write of the cached output port).
 *
 * @param channel  EXP_* channel index (0..7); must be configured as an output.
 * @param level    true = high, false = low.
 * @return ESP_OK on success, or the I2C error.
 */
esp_err_t gpio_expander_set(uint8_t channel, bool level);

/**
 * @brief Read one input channel's logic level.
 *
 * @param channel     EXP_* channel index (0..7).
 * @param[out] level  Receives the pin level.
 * @return ESP_OK on success, or the I2C error.
 */
esp_err_t gpio_expander_get(uint8_t channel, bool *level);

/**
 * @brief Read all 8 input-port levels in one transaction.
 *
 * @param[out] inputs  Receives the raw input port byte (bit n = channel n).
 * @return ESP_OK on success, or the I2C error.
 */
esp_err_t gpio_expander_read_all(uint8_t *inputs);

/** @} */
