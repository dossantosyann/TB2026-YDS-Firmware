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
#include "audio_sink.h"

/**
 * @brief Bring up the wired path's I2S bus; the sink owns the channel handle.
 *
 * Mounts I2S0 (BCK/WS/DOUT, no MCLK) and keeps the TX handle inside the sink, so upper
 * layers never touch it. Call once at startup. The PCM5242 I2C control port and the
 * expander mute/amp lines are initialised by their own drivers (audio_dac_init,
 * gpio_expander_init), not here.
 *
 * @return ESP_OK on success, or the I2S driver error.
 */
esp_err_t sink_i2s_dac_init(void);

/**
 * @brief Get the wired-DAC sink (I2S → PCM5242 → MAX97220 → jack).
 *
 * @return Pointer to the static sink vtable (never NULL). Requires sink_i2s_dac_init().
 */
const audio_sink_t *sink_i2s_dac_get(void);

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
