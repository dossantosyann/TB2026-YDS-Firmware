#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2s_std.h"

/* Brings up I2S0 as master TX to the PCM5242 (no MCLK; DAC derives its clock from
   BCLK). Comes up at a default rate/width; the decoder retunes per track. */
esp_err_t i2s_bus_init(i2s_chan_handle_t *out_tx_handle);

/* Retune the TX channel for a track. rate_hz: 8000..192000; bits: 16 (MP3) or 24
   (WAV) -- both ride MSB-justified in a fixed 32-bit slot. Disables the channel,
   reconfigures clock + slot, re-enables. */
esp_err_t i2s_bus_reconfig(i2s_chan_handle_t tx, uint32_t rate_hz, uint8_t bits);
