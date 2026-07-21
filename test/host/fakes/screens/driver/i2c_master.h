#pragma once
/* Minimal host stand-in for ESP-IDF's driver/i2c_master.h: just the opaque handle types and
   the one probe function stats_screen.c calls directly. No real I2C on the host. */
#include "esp_err.h"
#include <stdint.h>

typedef struct i2c_master_bus_t   *i2c_master_bus_handle_t;
typedef struct i2c_master_dev_t   *i2c_master_dev_handle_t;

esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus_handle, uint16_t address, int xfer_timeout_ms);
