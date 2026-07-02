/**
 * @file volume.c
 * @brief Volume service: pot -> active output (DAC byte or Bluetooth AVRCP), deadband write.
 * @ingroup services_audio_volume
 */
#include "volume.h"
#include "adc.h"
#include "audio_dac.h"
#include "settings.h"

/* Balance persisted in NVS as a biased u8 (steps + BAL_BIAS) so a signed trim fits an unsigned
   store; BAL_BIAS is also the plausible-range half-width, i.e. steps stay within [-40, +40]
   (±20 dB at 0.5 dB/step, matching the audio-settings slider). */
#define BAL_KEY   "audio_bal"
#define BAL_BIAS  40

static adc_oneshot_unit_handle_t s_adc;
static bool    s_adc_ready;                /* pot ADC unit brought up (independent of full init) */
static bool    s_ready;
static int8_t  s_balance;                 /* L/R trim in DAC volume register steps */
static uint8_t s_last_l = 0xFF, s_last_r = 0xFF;  /* 0xFF sentinel: first write always goes */
static uint8_t s_last_bt = 0xFF;          /* last AVRCP value sent (0xFF: impossible -> first goes) */
static volume_output_t s_output = VOLUME_OUT_DAC;
static esp_err_t (*s_bt_set)(uint8_t);    /* Bluetooth setter, registered to avoid a hard BT link */

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

/* Bring up the pot ADC once, shared by volume_init() and the diagnostic volume_read_mv(): either
   may run first (volume_read_mv is used before the audio path is wired), so init is idempotent. */
static esp_err_t ensure_adc(void)
{
    if (s_adc_ready) return ESP_OK;
    esp_err_t err = adc_pot_init(&s_adc);
    if (err == ESP_OK) s_adc_ready = true;
    return err;
}

esp_err_t volume_init(void)
{
    if (s_ready) return ESP_OK;            /* idempotent */

    esp_err_t err = ensure_adc();
    if (err != ESP_OK) return err;

    /* Restore the persisted L/R balance trim (default 0 when never set / out of range). */
    uint8_t stored;
    if (settings_get_u8(BAL_KEY, &stored) == ESP_OK && stored <= 2 * BAL_BIAS)
        s_balance = (int8_t)((int)stored - BAL_BIAS);

    s_ready = true;
    volume_poll(NULL, NULL);               /* push the initial knob position to the active output */
    return ESP_OK;
}

bool volume_read_mv(int *out_mv)
{
    if (ensure_adc() != ESP_OK) return false;   /* lazily owns the ADC if volume_init() hasn't run */
    return adc_pot_read(s_adc, out_mv) == ESP_OK;
}

bool volume_poll(int *out_mv, uint8_t *out_level)
{
    if (!s_ready) return false;

    int mv = 0;
    if (adc_pot_read(s_adc, &mv) != ESP_OK) return false;
    if (out_mv) *out_mv = mv;

    switch (s_output) {
    case VOLUME_OUT_DAC: {
        uint8_t l, r;
        apply_balance(adc_pot_to_volume(mv), &l, &r);
        if (out_level) *out_level = l;
        if (l == s_last_l && r == s_last_r) return false;       /* deadband: no I2C write */
        if (audio_dac_set_volume(l, r) != ESP_OK) return false;
        s_last_l = l;
        s_last_r = r;
        return true;
    }
    case VOLUME_OUT_BT: {
        uint8_t v = adc_pot_to_avrcp_volume(mv);
        if (out_level) *out_level = v;
        if (v == s_last_bt) return false;                       /* deadband: no AVRCP traffic */
        if (!s_bt_set || s_bt_set(v) != ESP_OK) return false;
        s_last_bt = v;
        return true;
    }
    case VOLUME_OUT_NONE:
    default:
        if (out_level) *out_level = 0;
        return false;                                           /* poll the pot, send nothing */
    }
}

void volume_set_output(volume_output_t out)
{
    if (out == s_output) return;
    s_output = out;
    s_last_l = s_last_r = 0xFF;            /* force the next poll to re-apply the knob to... */
    s_last_bt = 0xFF;                      /* ...the newly selected output */
}

volume_output_t volume_get_output(void)
{
    return s_output;
}

void volume_set_bt_handler(esp_err_t (*set_abs_vol)(uint8_t))
{
    s_bt_set = set_abs_vol;
}

void volume_set_balance(int8_t steps)
{
    s_balance = steps;
    s_last_l = s_last_r = 0xFF;            /* force a re-apply on the next poll */
}

int8_t volume_get_balance(void)
{
    return s_balance;
}

void volume_save_balance(void)
{
    settings_set_u8(BAL_KEY, (uint8_t)(s_balance + BAL_BIAS));
}
