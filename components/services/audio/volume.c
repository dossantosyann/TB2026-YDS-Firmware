/**
 * @file volume.c
 * @brief Volume service: pot -> DAC byte (deadband write) with an L/R balance trim.
 * @ingroup services_audio_volume
 */
#include "volume.h"
#include "adc.h"
#include "audio_dac.h"

static adc_oneshot_unit_handle_t s_adc;
static bool    s_ready;
static int8_t  s_balance;                 /* L/R trim in DAC volume register steps */
static uint8_t s_last_l = 0xFF, s_last_r = 0xFF;  /* 0xFF sentinel: first write always goes */

/* Split the knob level into per-channel bytes. Lower byte = louder, so attenuating a channel
   means raising its byte. balance > 0 attenuates right, < 0 attenuates left. Clamp to
   [0, 0xFE] to stay below 0xFF (hardware mute). */
static void apply_balance(uint8_t base, uint8_t *l, uint8_t *r)
{
    int li = base, ri = base;
    if (s_balance >= 0) ri += s_balance;
    else                li += -s_balance;
    if (li > 0xFE) li = 0xFE;
    if (ri > 0xFE) ri = 0xFE;
    *l = (uint8_t)li;
    *r = (uint8_t)ri;
}

esp_err_t volume_init(void)
{
    if (s_ready) return ESP_OK;            /* idempotent */

    esp_err_t err = adc_pot_init(&s_adc);
    if (err != ESP_OK) return err;

    s_ready = true;
    volume_poll(NULL, NULL);               /* push the initial knob position to the DAC */
    return ESP_OK;
}

bool volume_poll(int *out_mv, uint8_t *out_level)
{
    if (!s_ready) return false;

    int mv = 0;
    if (adc_pot_read(s_adc, &mv) != ESP_OK) return false;

    uint8_t l, r;
    apply_balance(adc_pot_to_volume(mv), &l, &r);

    if (out_mv)    *out_mv = mv;
    if (out_level) *out_level = l;

    if (l == s_last_l && r == s_last_r) return false;            /* deadband: no I2C write */
    if (audio_dac_set_volume(l, r) != ESP_OK) return false;

    s_last_l = l;
    s_last_r = r;
    return true;
}

void volume_set_balance(int8_t steps)
{
    s_balance = steps;
    s_last_l = s_last_r = 0xFF;            /* force a re-apply on the next poll */
}
