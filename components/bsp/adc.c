#include "adc.h"
#include "board_pins.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <math.h>

/* Wiper voltage at the ADC pin across the knob's full travel, measured on the board (mV). */
#define POT_MV_MIN    (147)
#define POT_MV_MAX    (2518)

/* Set to 1 if the wiper is wired upside down (min voltage = max volume). */
#define POT_INVERTED  (0)

/* Samples averaged per read to smooth pot/ADC noise. */
#define POT_SAMPLES   (16)

/* PCM5242 digital-volume scale: register 0x00 = +24 dB, -0.5 dB per step. */
#define DAC_GAIN_MAX_DB   (24.0f)
#define DAC_DB_PER_STEP   (0.5f)

/* Quietest gain the knob reaches at minimum, instead of the DAC's true mute. */
#define POT_FLOOR_DB      (-60)

static adc_cali_handle_t s_cali = NULL;

esp_err_t adc_pot_init(adc_oneshot_unit_handle_t *out_handle)
{
    const adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,                  // GPIO36 lives on ADC1
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, out_handle);
    if (err != ESP_OK) return err;

    const adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,               // ~0..3.1 V full scale, covers the 2.518 V top
        .bitwidth = ADC_BITWIDTH_DEFAULT,       // 12-bit on ESP32
    };
    err = adc_oneshot_config_channel(*out_handle, POT_ADC_CHANNEL, &chan_cfg);
    if (err != ESP_OK) return err;

    // Turns raw counts into real millivolts using the chip's factory eFuse data.
    const adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    return adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali);
}

esp_err_t adc_pot_read(adc_oneshot_unit_handle_t handle, int *out_mv)
{
    int sum = 0;
    for (int i = 0; i < POT_SAMPLES; i++) {
        int raw;
        esp_err_t err = adc_oneshot_read(handle, POT_ADC_CHANNEL, &raw);
        if (err != ESP_OK) return err;
        sum += raw;
    }
    return adc_cali_raw_to_voltage(s_cali, sum / POT_SAMPLES, out_mv);
}

uint8_t adc_pot_to_volume(int mv)
{
    float vol = (float)(mv - POT_MV_MIN) / (float)(POT_MV_MAX - POT_MV_MIN);
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
#if POT_INVERTED
    vol = 1.0f - vol;
#endif
    // Louder = lower register byte: vol=1 -> 0x00 (+24 dB), vol=0 -> POT_FLOOR_DB.
    const float floor_reg = (DAC_GAIN_MAX_DB - POT_FLOOR_DB) / DAC_DB_PER_STEP;
    return (uint8_t)lroundf((1.0f - vol) * floor_reg);
}
