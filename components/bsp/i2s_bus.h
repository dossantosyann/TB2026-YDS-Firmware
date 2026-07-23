/**
 * @file i2s_bus.h
 * @brief I2S0 master TX stream to the PCM5242 DAC.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2s_std.h"

/**
 * @defgroup bsp_i2s I2S bus
 * @ingroup bsp
 * @brief I2S0 master TX stream to the PCM5242 DAC.
 * @{
 */

/**
 * @brief Bring up I2S0 as master TX to the PCM5242.
 *
 * No MCLK is routed; the DAC derives its clock from BCLK. The channel comes up at a
 * default rate/width; the decoder retunes it per track via i2s_bus_reconfig().
 *
 * @param[out] out_tx_handle  Receives the enabled TX channel handle.
 * @return ESP_OK on success, or the underlying driver error.
 */
esp_err_t i2s_bus_init(i2s_chan_handle_t *out_tx_handle);

/**
 * @brief Retune the TX channel for a track: disable, reconfigure clock + slot, re-enable.
 *
 * Both widths ride MSB-justified in a fixed 32-bit slot.
 *
 * @param tx       TX channel handle from i2s_bus_init().
 * @param rate_hz  Sample rate, 8000..192000.
 * @param bits     Source sample width; currently ignored — the decoders always emit
 *                 32-bit MSB-justified words, so the data width stays 32-bit.
 * @return ESP_OK on success, or the underlying driver error.
 */
esp_err_t i2s_bus_reconfig(i2s_chan_handle_t tx, uint32_t rate_hz, uint8_t bits);

/** @} */
