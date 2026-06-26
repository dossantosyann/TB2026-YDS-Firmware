/**
 * @file volume.h
 * @brief Volume service: the sole writer of the PCM5242 digital volume.
 *
 * Owns the volume potentiometer (ADC1, see @ref bsp_adc) and is the only place that calls
 * audio_dac_set_volume(). It samples the knob, maps it to a DAC register byte and writes
 * the DAC only when that byte changes, so a still knob costs no I2C traffic. The driver was
 * made independent L/R on purpose; this service applies an optional balance trim on top of
 * the knob level to even out the analog L/R mismatch downstream of the DAC.
 *
 * Not real-time: drive it from a periodic maintenance/UI loop via volume_poll(), not its own
 * task. The Bluetooth/AVRCP path will feed the same service once it lands.
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>

/**
 * @defgroup services_audio_volume Volume service
 * @ingroup services
 * @brief Sole writer of the PCM5242 digital volume (pot -> DAC, with L/R balance).
 * @{
 */

/**
 * @brief Bring up the volume pot and push the current knob position to the DAC.
 *
 * Call once at startup, after audio_dac_init() (this service writes the DAC). Owns the ADC
 * unit handle internally. Idempotent.
 *
 * @return ESP_OK; otherwise the underlying ADC init error.
 */
esp_err_t volume_init(void);

/**
 * @brief Sample the pot and write the DAC only if the mapped level changed.
 *
 * Call periodically (~10..20 Hz) from a maintenance/UI loop. The ADC read happens every
 * call (cheap); the I2C write is skipped when the level is unchanged (deadband).
 *
 * @param[out] out_mv     Optional: wiper voltage in millivolts (for display). May be NULL.
 * @param[out] out_level  Optional: applied left-channel volume byte (for display). May be NULL.
 * @return true if a new level was written to the DAC this call, false otherwise.
 */
bool volume_poll(int *out_mv, uint8_t *out_level);

/**
 * @brief Trim the L/R balance to compensate the analog imbalance (driver is independent L/R).
 *
 * Applied on top of the knob level from the next poll onward. Each step is one DAC volume
 * register step (0.5 dB). Positive attenuates the RIGHT channel (the louder analog side on
 * this board), negative attenuates the LEFT. 0 = equal.
 *
 * @param steps  Signed register-step offset.
 */
void volume_set_balance(int8_t steps);

/** @} */
