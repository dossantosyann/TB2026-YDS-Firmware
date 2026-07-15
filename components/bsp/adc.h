/**
 * @file adc.h
 * @brief Volume potentiometer: ADC1 sampling and mapping to PCM5242 digital volume.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

/**
 * @defgroup bsp_adc ADC / Volume potentiometer
 * @ingroup bsp
 * @brief Potentiometer sampling on ADC1 and mapping to PCM5242 digital volume.
 * @{
 */

/**
 * @brief Bring up ADC1 for the volume potentiometer (GPIO36) with line-fitting calibration.
 *
 * @param[out] out_handle  Receives the oneshot unit handle; owned by the caller.
 * @return ESP_OK on success, or the underlying ADC unit/channel/calibration error.
 */
esp_err_t adc_pot_init(adc_oneshot_unit_handle_t *out_handle);

/**
 * @brief Read the averaged, calibrated wiper voltage.
 *
 * @param      handle  Unit handle from adc_pot_init().
 * @param[out] out_mv  Receives the wiper voltage in millivolts.
 * @return ESP_OK on success; propagates the ADC read error.
 */
esp_err_t adc_pot_read(adc_oneshot_unit_handle_t handle, int *out_mv);

/**
 * @brief Map a wiper voltage to a PCM5242 digital-volume register byte.
 *
 * 0x00 = +24 dB (max), 0x30 = 0 dB, 0xFE = -103 dB, 0xFF = mute.
 *
 * @param mv  Wiper voltage in millivolts.
 * @return The digital-volume register byte. Pure function, never fails.
 */
uint8_t adc_pot_to_volume(int mv);

/**
 * @brief Map a wiper voltage to an AVRCP absolute-volume value (0..0x7F) for a Bluetooth sink.
 *
 * Linear in the knob position: minimum -> 0 (silent), maximum -> 0x7F (loudest). Unlike
 * adc_pot_to_volume(), there is no PCM5242 dB curve or -60 dB floor — the speaker applies its
 * own curve, and the bottom of the knob reaches true silence.
 *
 * @param mv  Wiper voltage in millivolts.
 * @return The AVRCP absolute-volume value, 0..0x7F. Pure function, never fails.
 */
uint8_t adc_pot_to_avrcp_volume(int mv);

/**
 * @brief Map a 0..1 volume fraction directly to a PCM5242 register byte, bypassing the pot.
 *
 * Same curve as adc_pot_to_volume() (it feeds the fraction through the knob's mv range), so a
 * fixed level matches what the equivalent knob position would produce. For deterministic tests
 * that must not depend on the physical wiper.
 *
 * @param frac  Volume fraction, 0.0 (quietest) .. 1.0 (loudest); clamped.
 * @return The digital-volume register byte. Pure function, never fails.
 */
uint8_t adc_frac_to_volume(float frac);

/**
 * @brief Map a 0..1 volume fraction directly to an AVRCP absolute-volume value, bypassing the pot.
 * @param frac  Volume fraction, 0.0 (silent) .. 1.0 (loudest); clamped.
 * @return The AVRCP absolute-volume value, 0..0x7F. Pure function, never fails.
 */
uint8_t adc_frac_to_avrcp_volume(float frac);

/** @} */
