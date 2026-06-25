/**
 * @file sink_i2s_dac.h
 * @brief Wired output path control: PCM5242 analog mute (XSMT) and MAX97220 amp shutdown.
 *
 * Both lines are GPIO-expander pins, not chip data interfaces, so this routing lives in
 * the audio service rather than in the drivers: the DAC driver stays I2C-only and the amp
 * has no driver of its own. Power sequencing between the two is this service's concern.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Enable or mute the DAC analog output via the PCM5242 XSMT pin.
 *
 * XSMT is expander IO0, active-low. Requires gpio_expander_init() to have run first.
 *
 * @param enable  true = drive XSMT high (output on), false = XSMT low (muted).
 * @return ESP_OK on success, or the expander I2C error.
 */
esp_err_t sink_i2s_dac_output_enable(bool enable);

/**
 * @brief Enable or shut down the MAX97220 headphone amplifier via its SHDN pin.
 *
 * SHDN is expander IO1, active-low. Requires gpio_expander_init() to have run first.
 *
 * @param enable  true = drive SHDN high (amp on), false = SHDN low (shutdown).
 * @return ESP_OK on success, or the expander I2C error.
 */
esp_err_t sink_i2s_dac_amp_enable(bool enable);
