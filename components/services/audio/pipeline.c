/**
 * @file pipeline.c
 * @brief Audio pipeline task: producer->consumer loop from a PCM source to a selectable sink
 *        (wired DAC or Bluetooth).
 * @ingroup services_audio_pipeline
 */
#include "pipeline.h"
#include "audio_sink.h"
#include "sink_i2s_dac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <math.h>
#include <stdlib.h>

static const char *TAG = "pipeline";

/* Wired-path bring-up format: I2S default rate, 16-bit stereo (see i2s_bus_init). */
#define PIPE_FS            44100
#define PIPE_CHUNK_FRAMES  512          /* PCM frames handed to sink->write() per iteration */
#define TONE_AMPLITUDE     10000        /* ~ -10 dBFS, safe on headphones */
#define TONE_VOLUME        0x68         /* placeholder DAC volume byte (~ -20 dB) until the
                                           volume service lands; equal L/R (the analog L/R
                                           imbalance is a hardware matter, not firmware's). */

#define PIPE_TASK_PRIO     22           /* above UI/maintenance, below IDF system tasks */
#define PIPE_TASK_CORE     1            /* APP_CPU: keep the audio loop off core 0 (Wi-Fi/BT) */
#define PIPE_TASK_STACK    4096

enum { CMD_PLAY_TONE, CMD_STOP };

typedef struct {
    uint8_t  cmd;
    uint32_t freq;
} pipe_cmd_t;

static QueueHandle_t s_cmd_q;
static int16_t       s_chunk[PIPE_CHUNK_FRAMES * 2];   /* interleaved stereo write buffer */
static const audio_sink_t *s_sink;                     /* output backend; defaults to the wired DAC */

/* Stream a sine until a CMD_STOP arrives. One period is precomputed once: the LX6 FPU is
   single-precision, so a per-sample double sin() would be software-emulated and starve the
   I2S DMA. The loop then walks that table with a phase that persists across write buffers,
   so successive chunks join without a discontinuity. */
static void run_tone(uint32_t freq_hz)
{
    const int period = PIPE_FS / freq_hz;          /* >= 2, validated by the caller */
    int16_t *table = malloc((size_t)period * sizeof(int16_t));
    if (!table) {
        ESP_LOGE(TAG, "tone: out of memory for %d-sample table", period);
        return;
    }
    for (int i = 0; i < period; i++) {
        table[i] = (int16_t)(sin(2.0 * 3.14159265358979 * i / period) * TONE_AMPLITUDE);
    }

    const audio_sink_t *sink = s_sink;
    esp_err_t err = sink->start(PIPE_FS, 16, 2);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tone: sink start failed (%s)", esp_err_to_name(err));
        free(table);
        return;
    }
    sink->set_volume(TONE_VOLUME, TONE_VOLUME);
    ESP_LOGI(TAG, "tone: %u Hz streaming (period %d samples)", (unsigned)(PIPE_FS / period), period);

    int phase = 0;
    pipe_cmd_t c;
    for (;;) {
        for (int f = 0; f < PIPE_CHUNK_FRAMES; f++) {
            int16_t s = table[phase];
            s_chunk[2 * f]     = s;   /* L */
            s_chunk[2 * f + 1] = s;   /* R */
            if (++phase >= period) phase = 0;
        }
        size_t written;
        sink->write(s_chunk, sizeof s_chunk, &written);

        if (xQueueReceive(s_cmd_q, &c, 0) && c.cmd == CMD_STOP) break;
    }

    sink->stop();
    free(table);
    ESP_LOGI(TAG, "tone: stopped");
}

static void audio_task(void *arg)
{
    (void)arg;
    pipe_cmd_t c;
    for (;;) {
        /* Idle: block until something asks to play. A CMD_STOP received here is a no-op. */
        if (xQueueReceive(s_cmd_q, &c, portMAX_DELAY) && c.cmd == CMD_PLAY_TONE) {
            run_tone(c.freq);
        }
    }
}

esp_err_t pipeline_init(void)
{
    if (s_cmd_q) return ESP_OK;        /* idempotent */

    s_sink = sink_i2s_dac_get();       /* default output; pipeline_set_sink() can switch it */

    s_cmd_q = xQueueCreate(4, sizeof(pipe_cmd_t));
    if (!s_cmd_q) return ESP_ERR_NO_MEM;

    BaseType_t ok = xTaskCreatePinnedToCore(audio_task, "audio", PIPE_TASK_STACK,
                                            NULL, PIPE_TASK_PRIO, NULL, PIPE_TASK_CORE);
    if (ok != pdPASS) {
        vQueueDelete(s_cmd_q);
        s_cmd_q = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void pipeline_set_sink(const audio_sink_t *sink)
{
    if (sink) s_sink = sink;
}

esp_err_t pipeline_play_tone(uint32_t freq_hz)
{
    if (!s_cmd_q) return ESP_ERR_INVALID_STATE;
    if (freq_hz < 20 || freq_hz > PIPE_FS / 2) return ESP_ERR_INVALID_ARG;

    pipe_cmd_t c = { .cmd = CMD_PLAY_TONE, .freq = freq_hz };
    return xQueueSend(s_cmd_q, &c, 0) == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t pipeline_stop(void)
{
    if (!s_cmd_q) return ESP_ERR_INVALID_STATE;

    pipe_cmd_t c = { .cmd = CMD_STOP };
    return xQueueSend(s_cmd_q, &c, 0) == pdPASS ? ESP_OK : ESP_FAIL;
}
