/**
 * @file audio_dac.h
 * @brief PCM5242 DAC control over I2C (volume, mute).
 *
 * SCK is grounded on this board, so the DAC cannot auto-clock; this driver programs the
 * PLL and clock dividers off BCK by hand (see audio_dac_set_sample_rate()). It otherwise
 * only handles the I2C control port at address 0x4C. Mute also depends on the XSMT pin
 * on the GPIO expander, which this driver does not touch.
 */
#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

/**
 * @defgroup drivers_audio_dac Audio DAC (PCM5242)
 * @ingroup drivers
 * @brief I2C control of the PCM5242 digital volume and soft mute.
 * @{
 */

/**
 * @brief Attach the PCM5242 to the shared I2C bus and select independent L/R volume.
 *
 * Selects register page 0 and sets the volume-control mode so the left (0x3D) and right
 * (0x3E) digital volumes are independent, letting audio_dac_set_volume() apply an L/R
 * balance. Higher layers compute the per-channel bytes; the driver just writes them.
 *
 * @param bus  Master bus handle from i2c_bus_init().
 * @return ESP_OK on success, or the I2C error (e.g. if the DAC does not ACK).
 */
esp_err_t audio_dac_init(i2c_master_bus_handle_t bus);

/**
 * @brief Retune the PLL and clock dividers for a new sample rate.
 *
 * Call this whenever the track's sample rate changes (alongside i2s_bus_reconfig()).
 * BCK is the PLL reference (= 64*fS), so rates below 16 kHz drive BCK under the PLL's
 * 1 MHz input minimum and cannot lock; they are rejected. Supported: 16, 22.05, 24, 32,
 * 44.1, 48, 88.2, 96, 176.4, 192 kHz.
 *
 * Holds the DAC in standby across the PLL rewrite and powers it back up itself, so the
 * call is safe mid-playback. It does not soft-mute: ramp audio_dac_mute()/the amp around
 * the call yourself if you need a pop-free transition.
 *
 * @param rate_hz  Sample rate in Hz.
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED for an unsupported rate, or the I2C error.
 */
esp_err_t audio_dac_set_sample_rate(uint32_t rate_hz);

/**
 * @brief Set the left and right digital volumes independently (see audio_dac_init()).
 *
 * Pass the same byte for both to keep the channels matched, or different bytes to apply
 * an L/R balance (e.g. to compensate an analog channel-gain mismatch).
 *
 * @param left   Left-channel volume byte: 0x00 = +24 dB, 0x30 = 0 dB, 0xFE = -103 dB,
 *               0xFF = mute (see adc_pot_to_volume()).
 * @param right  Right-channel volume byte, same scale.
 * @return ESP_OK on success, or the I2C error.
 */
esp_err_t audio_dac_set_volume(uint8_t left, uint8_t right);

/**
 * @brief Read the left-channel digital volume byte back, for verification.
 *
 * @param[out] level  Receives the left-channel volume register value.
 * @return ESP_OK on success, or the I2C error.
 */
esp_err_t audio_dac_get_volume(uint8_t *level);

/**
 * @brief Soft-mute or un-mute both channels (ramped, pop-free).
 *
 * @param mute  true to mute, false to un-mute.
 * @return ESP_OK on success, or the I2C error.
 */
esp_err_t audio_dac_mute(bool mute);

/**
 * @brief Request or release software standby (DAC and line driver powered down).
 *
 * Standby keeps the charge pump and digital supply alive for a quick resume. Power
 * sequencing relative to the amplifier is the caller's concern, not this driver's.
 *
 * @param standby  true to enter standby, false to return to normal operation.
 * @return ESP_OK on success, or the I2C error.
 */
esp_err_t audio_dac_standby(bool standby);

/**
 * @brief Read the clock/PLL status register (0x5E) for bring-up diagnostics.
 *
 * Each flag is 0 when valid: D5 PLL unlocked, D2 SCK invalid, D1 BCK invalid,
 * D0 fS invalid. A value near 0x00 means the DAC sees valid clocks and the PLL is
 * locked; high bits mean missing/invalid clocks (e.g. SCK not grounded).
 *
 * @param[out] status  Receives the raw register byte.
 * @return ESP_OK on success, or the I2C error.
 */
esp_err_t audio_dac_get_clock_status(uint8_t *status);

/** @} */
