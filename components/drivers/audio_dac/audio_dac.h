/**
 * @file audio_dac.h
 * @brief PCM5242 DAC control over I2C (volume, mute).
 *
 * The DAC clocks itself from the I2S bus (auto-clock, see bsp/i2s_bus.c); this driver
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
 * @brief Attach the PCM5242 to the shared I2C bus and link the right volume to the left.
 *
 * Selects register page 0 and sets the volume-control mode so the right channel tracks
 * the left, letting audio_dac_set_volume() drive both with a single register write.
 *
 * @param bus  Master bus handle from i2c_bus_init().
 * @return ESP_OK on success, or the I2C error (e.g. if the DAC does not ACK).
 */
esp_err_t audio_dac_init(i2c_master_bus_handle_t bus);

/**
 * @brief Set the digital volume; the right channel follows the left (see audio_dac_init()).
 *
 * @param level  PCM5242 volume byte: 0x00 = +24 dB, 0x30 = 0 dB, 0xFE = -103 dB,
 *               0xFF = mute (see adc_pot_to_volume()).
 * @return ESP_OK on success, or the I2C error.
 */
esp_err_t audio_dac_set_volume(uint8_t level);

/**
 * @brief Read the digital volume byte back (left channel), for verification.
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

/** @} */
