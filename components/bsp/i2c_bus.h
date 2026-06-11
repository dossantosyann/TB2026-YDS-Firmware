#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

/* Per-device SCL speed, set when drivers add themselves to the bus. */
#define I2C_BUS_FREQ_HZ (400 * 1000)

esp_err_t i2c_bus_init(i2c_master_bus_handle_t *out_handle);

/* Probe 0x08..0x77 and log every ACKing address. Bring-up helper. */
void i2c_bus_scan(i2c_master_bus_handle_t bus);
