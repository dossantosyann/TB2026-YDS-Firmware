#pragma once

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

esp_err_t adc_pot_init(adc_oneshot_unit_handle_t *out_handle);
int       adc_pot_read(adc_oneshot_unit_handle_t handle);
