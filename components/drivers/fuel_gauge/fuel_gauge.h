#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

/* MAX17260 */
typedef struct {
    float   voltage_v;
    float   current_ma;
    uint8_t soc_pct;
} fuel_gauge_data_t;

esp_err_t fuel_gauge_init(i2c_master_bus_handle_t i2c);
esp_err_t fuel_gauge_read(fuel_gauge_data_t *out);
