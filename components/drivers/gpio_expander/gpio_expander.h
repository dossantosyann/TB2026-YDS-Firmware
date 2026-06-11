#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

/* PI4IOE5V9554A — 8-bit I2C GPIO expander (PCA9554A-compatible), 7-bit addr 0x38 */
esp_err_t gpio_expander_init(i2c_master_bus_handle_t i2c);
esp_err_t gpio_expander_set_pin(uint8_t pin, int level);
int       gpio_expander_get_pin(uint8_t pin);
