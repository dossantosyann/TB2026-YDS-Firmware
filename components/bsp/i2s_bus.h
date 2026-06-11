#pragma once

#include "esp_err.h"
#include "driver/i2s_std.h"

esp_err_t i2s_bus_init(i2s_chan_handle_t *out_tx_handle);
