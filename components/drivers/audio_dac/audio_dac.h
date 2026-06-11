#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

/* PCM5242 — I2C control + I2S data */
esp_err_t audio_dac_init(i2c_master_bus_handle_t i2c);
esp_err_t audio_dac_set_volume(uint8_t volume);
