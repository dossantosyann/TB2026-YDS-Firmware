#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

esp_err_t adc_pot_init(adc_oneshot_unit_handle_t *out_handle);

/* Averaged, calibrated wiper voltage in mV via out_mv. Propagates the ADC read error. */
esp_err_t adc_pot_read(adc_oneshot_unit_handle_t handle, int *out_mv);

/* Maps wiper voltage (mV) to a PCM5242 digital-volume register byte:
   0x00 = +24 dB (max), 0x30 = 0 dB, 0xFE = -103 dB, 0xFF = mute. Pure, never fails. */
uint8_t adc_pot_to_volume(int mv);
