#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

/* FXL6408D — 8-bit I2C GPIO expander */
esp_err_t gpio_expander_init(i2c_master_bus_handle_t i2c);
esp_err_t gpio_expander_set_pin(uint8_t pin, int level);
int       gpio_expander_get_pin(uint8_t pin);
