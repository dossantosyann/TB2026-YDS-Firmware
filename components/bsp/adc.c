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

/* Shared by the channel and the calibration: the cali atten must match the channel's
   or adc_pot_read returns wrong millivolts. Keep them defined in one place. */
#define POT_ATTEN     ADC_ATTEN_DB_12       // ~0..3.1 V full scale, covers the 2.518 V top
#define POT_BITWIDTH  ADC_BITWIDTH_DEFAULT  // 12-bit on ESP32

/* PCM5242 digital-volume scale: register 0x00 = +24 dB, -0.5 dB per step. */
#define DAC_REG0_DB       (24.0f)   /* hardware fact: register 0x00 = +24 dB */
#define DAC_DB_PER_STEP   (0.5f)

/* Loudest gain the knob reaches at maximum. Capped at 0 dB (not the +24 dB the DAC allows)
   to avoid the harsh, ear-damaging saturation of positive digital gain. */
#define DAC_MAX_DB        (-20.0f)

/* Quietest gain the knob reaches at minimum, instead of the DAC's true mute. */
#define POT_FLOOR_DB      (-60.0f)

static adc_cali_handle_t s_cali = NULL;

esp_err_t adc_pot_init(adc_oneshot_unit_handle_t *out_handle)
{
    const adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,                  // GPIO36 lives on ADC1
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, out_handle);
    if (err != ESP_OK) return err;

    const adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = POT_ATTEN,
        .bitwidth = POT_BITWIDTH,
    };
    err = adc_oneshot_config_channel(*out_handle, POT_ADC_CHANNEL, &chan_cfg);
    if (err != ESP_OK) return err;

    // Turns raw counts into real millivolts using the chip's factory eFuse data.
    const adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = POT_ATTEN,
        .bitwidth = POT_BITWIDTH,
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
    // Louder = lower register byte. vol=1 -> ceil_reg (DAC_MAX_DB), vol=0 -> floor_reg (POT_FLOOR_DB).
    const float ceil_reg  = (DAC_REG0_DB - DAC_MAX_DB)   / DAC_DB_PER_STEP;
    const float floor_reg = (DAC_REG0_DB - POT_FLOOR_DB) / DAC_DB_PER_STEP;
    return (uint8_t)lroundf(floor_reg - vol * (floor_reg - ceil_reg));
}

uint8_t adc_pot_to_avrcp_volume(int mv)
{
    float vol = (float)(mv - POT_MV_MIN) / (float)(POT_MV_MAX - POT_MV_MIN);
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
#if POT_INVERTED
    vol = 1.0f - vol;
#endif
    // Linear: louder = higher value. vol=0 -> 0 (silent), vol=1 -> 0x7F (loudest).
    return (uint8_t)lroundf(vol * 0x7F);
}

/* Feed a fraction through the knob's mv range so the fixed level uses the exact same curve
   (dB shaping, floor, POT_INVERTED) as the pot — the single source of truth for the mapping. */
static int frac_to_mv(float frac)
{
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    return POT_MV_MIN + (int)lroundf(frac * (POT_MV_MAX - POT_MV_MIN));
}

uint8_t adc_frac_to_volume(float frac)
{
    return adc_pot_to_volume(frac_to_mv(frac));
}

uint8_t adc_frac_to_avrcp_volume(float frac)
{
    return adc_pot_to_avrcp_volume(frac_to_mv(frac));
}
