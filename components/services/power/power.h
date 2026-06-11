#pragma once

#include "esp_err.h"
#include "fuel_gauge.h"

esp_err_t power_init(void);
esp_err_t power_get_status(fuel_gauge_data_t *out);
