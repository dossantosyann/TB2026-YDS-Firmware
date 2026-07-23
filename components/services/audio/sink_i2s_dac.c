/**
 * @file sink_i2s_dac.c
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "audio_sink.h"
#include "audio_dac.h"
#include "sink_i2s_dac.h"
#include "gpio_expander.h"
#include "board_pins.h"
#include "i2s_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static i2s_chan_handle_t s_tx;   /* the sink owns the I2S bus handle */

esp_err_t sink_i2s_dac_output_enable(bool enable)
{
    /* PCM5242 XSMT (expander IO0) is active-low: high = un-mute, low = soft mute. */
    return gpio_expander_set(EXP_DAC_MUTE, enable);
}

esp_err_t sink_i2s_dac_amp_enable(bool enable)
{
    /* MAX97220 SHDN (expander IO1) is active-low: high = on, low = shutdown. */
    return gpio_expander_set(EXP_AMP_SHDN, enable);
}

esp_err_t sink_i2s_dac_init(void)
{
    return i2s_bus_init(&s_tx);
}

static esp_err_t i2s_start(uint32_t rate_hz, uint8_t bits, uint8_t channels)
{
    ESP_LOGI("i2s", "start: %lu Hz %u-bit", rate_hz, bits);

    (void)channels;   /* the wired path is fixed stereo (32-bit slots) */

    esp_err_t err = i2s_bus_reconfig(s_tx, rate_hz, bits);
    if (err != ESP_OK) return err;
    err = audio_dac_set_sample_rate(rate_hz);   /* relock the DAC PLL for the rate */
    if (err != ESP_OK) return err;

    audio_dac_standby(false);
    audio_dac_mute(false);

    /* Pop-free turn-on: un-mute the DAC analog output (XSMT) first, settle, amp last. */
    err = sink_i2s_dac_output_enable(true);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(20));
    return sink_i2s_dac_amp_enable(true);
}

static esp_err_t i2s_write(const void *pcm, size_t len, size_t *written)
{
    return i2s_channel_write(s_tx, pcm, len, written, portMAX_DELAY);
}

static esp_err_t i2s_stop(void)
{
    /* Reverse order: amp down first so its transient stays out of the cans, then mute,
       then standby. */
    esp_err_t err = sink_i2s_dac_amp_enable(false);
    audio_dac_mute(true);
    audio_dac_standby(true);
    return err;
}

static esp_err_t i2s_set_volume(uint8_t left, uint8_t right)
{
    return audio_dac_set_volume(left, right);
}

static const audio_sink_t s_sink = {
    .start      = i2s_start,
    .write      = i2s_write,
    .stop       = i2s_stop,
    .set_volume = i2s_set_volume,
};

const audio_sink_t *sink_i2s_dac_get(void)
{
    return &s_sink;
}
