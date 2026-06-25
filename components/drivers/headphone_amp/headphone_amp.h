/**
 * @file headphone_amp.h
 * @brief MAX97220 headphone amplifier — enable/shutdown via the GPIO expander.
 *
 * The amp has no data interface; its only control is the active-low SHDN pin on the
 * expander (EXP_AMP_SHDN). This wrapper hides that polarity behind an "enable" verb.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @defgroup drivers_headphone_amp Headphone amplifier (MAX97220)
 * @ingroup drivers
 * @brief Single-pin enable/shutdown of the headphone amp through the expander.
 * @{
 */

/**
 * @brief Enable or shut down the headphone amplifier.
 *
 * @param enable  true = drive SHDN high (amp on), false = SHDN low (shutdown).
 * @return ESP_OK on success, or the expander I2C error.
 */
esp_err_t headphone_amp_enable(bool enable);

/** @} */
