/**
 * @file pipeline.c
 * @brief Audio pipeline task: producer->consumer loop from a PCM source to a selectable sink
 *        (wired DAC or Bluetooth).
 * @ingroup services_audio_pipeline
 */
#include "pipeline.h"
#include "audio_sink.h"
#include "sink_i2s_dac.h"
#include "decoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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

#define PIPE_PATH_MAX      320          /* matches STORAGE_PATH_MAX: "/sdcard/" + LFN + nul */

enum { CMD_PLAY_TONE, CMD_PLAY_FILE, CMD_STOP, CMD_PAUSE, CMD_RESUME, CMD_SWITCH_SINK };

typedef struct {
    uint8_t  cmd;
    uint32_t freq;                      /* CMD_PLAY_TONE */
    char     path[PIPE_PATH_MAX];       /* CMD_PLAY_FILE */
} pipe_cmd_t;

static QueueHandle_t s_cmd_q;
static int32_t       s_chunk[PIPE_CHUNK_FRAMES * 2];   /* interleaved stereo tone buffer, 32-bit */
static uint8_t       s_file_buf[DECODER_READ_BUF_BYTES]; /* decode chunk buffer (32-bit stereo) */
static const audio_sink_t *s_sink;                     /* output backend; defaults to the wired DAC */
static void (*s_end_cb)(pipeline_end_reason_t);        /* track-end notify (player) */

/* Playback position. The audio task writes these while decoding; the UI task reads them via
   pipeline_get_position(). Each is a 32-bit aligned word (atomic load/store on the LX6), so a
   plain snapshot is tear-free without a lock. s_pos_rate == 0 means nothing is playing.
   s_pos_total_ms is set once before the decode loop; s_pos_frames advances per chunk and
   freezes naturally while paused (the loop sleeps). */
static volatile uint32_t s_pos_frames;     /* PCM frames played in the current file */
static volatile uint32_t s_pos_rate;       /* current file sample rate (0 = nothing playing) */
static volatile uint32_t s_pos_total_ms;   /* current file duration, 0 = unknown */

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
    esp_task_wdt_add(NULL);              /* guard the streaming loop against a stalled sink */
    for (;;) {
        esp_task_wdt_reset();
        for (int f = 0; f < PIPE_CHUNK_FRAMES; f++) {
            int16_t s = table[phase];
            s_chunk[2 * f]     = (int32_t)s << 16;   /* L, 32-bit MSB-justified */
            s_chunk[2 * f + 1] = (int32_t)s << 16;   /* R */
            if (++phase >= period) phase = 0;
        }
        size_t written;
        sink->write(s_chunk, sizeof s_chunk, &written);

        if (xQueueReceive(s_cmd_q, &c, 0) && c.cmd == CMD_STOP) break;
    }

    esp_task_wdt_delete(NULL);
    sink->stop();
    free(table);
    ESP_LOGI(TAG, "tone: stopped");
}

/* Decode a file to the current sink until EOF, CMD_STOP, or a decode error. Volume is the
   volume service's job during playback, so (unlike the tone) nothing here calls set_volume.
   CMD_PAUSE powers the sink down while keeping the decoder open at its position, then sleeps
   on the queue until CMD_RESUME (restart the sink, keep decoding) or CMD_STOP. On a natural
   end (EOF or error) the transport is notified so it can chain to the next track; a
   user-initiated stop is silent. */
static void run_file(const char *path)
{
    ESP_LOGI(TAG, "file: %s", path);

    if (decoder_open(path) != ESP_OK) {
        ESP_LOGE(TAG, "file: open failed");
        if (s_end_cb) s_end_cb(PIPE_END_ERROR);
        return;
    }

    decoder_format_t fmt;
    const audio_sink_t *sink = s_sink;
    if (decoder_format(&fmt) != ESP_OK) {
        ESP_LOGE(TAG, "file: format failed");
        decoder_close();
        if (s_end_cb) s_end_cb(PIPE_END_ERROR);
        return;
    }
    ESP_LOGI(TAG, "file: %lu Hz %u-bit %u-ch", fmt.rate_hz, fmt.bits, fmt.channels);
    esp_err_t serr = sink->start(fmt.rate_hz, fmt.bits, fmt.channels);
    if (serr != ESP_OK) {
        ESP_LOGE(TAG, "file: sink start failed (%s)", esp_err_to_name(serr));
        decoder_close();
        if (s_end_cb) s_end_cb(serr == ESP_ERR_NOT_SUPPORTED ? PIPE_END_UNSUPPORTED
                                                             : PIPE_END_ERROR);
        return;
    }

    pipeline_end_reason_t reason = PIPE_END_EOF;   /* default: ran to end of file */
    bool notify = true;                            /* a user CMD_STOP ends silently */
    bool sink_on = true;
    const uint32_t out_frame = 8;  /* 32-bit stereo: 4 B/sample × 2 ch */
    s_pos_frames   = 0;
    s_pos_total_ms = fmt.duration_ms;
    s_pos_rate     = fmt.rate_hz;                  /* set last: non-zero marks position valid */
    pipe_cmd_t c;
    size_t got = 0, off = 0;   /* current decode chunk and how much the sink accepted so far */
    esp_task_wdt_add(NULL);    /* guard the decode loop; released across pause and on exit below */
    for (;;) {
        esp_task_wdt_reset();
        if (xQueueReceive(s_cmd_q, &c, 0)) {
            if (c.cmd == CMD_STOP) { notify = false; break; }
            if (c.cmd == CMD_SWITCH_SINK) {
                /* Re-route mid-track onto the newly selected sink, keeping the decoder open so
                   the position (s_pos_*) carries over: the track continues where it was. Any
                   partially written chunk (off < got) is flushed to the new sink next. If the
                   new sink refuses the format (e.g. non-44.1 kHz over Bluetooth), end as the
                   play path would so the UI can say the track is not playable on this output. */
                sink->stop();
                sink = s_sink;
                serr = sink->start(fmt.rate_hz, fmt.bits, fmt.channels);
                if (serr != ESP_OK) {
                    reason = (serr == ESP_ERR_NOT_SUPPORTED) ? PIPE_END_UNSUPPORTED
                                                             : PIPE_END_ERROR;
                    break;
                }
            }
            if (c.cmd == CMD_PAUSE) {
                sink->stop();                      /* path down; decoder kept open */
                sink_on = false;
                esp_task_wdt_delete(NULL);         /* paused: legitimately idle, don't guard */
                do { xQueueReceive(s_cmd_q, &c, portMAX_DELAY); } while (c.cmd == CMD_PAUSE);
                esp_task_wdt_add(NULL);            /* resuming or stopping: guard again */
                if (c.cmd == CMD_STOP) { notify = false; break; }
                serr = sink->start(fmt.rate_hz, fmt.bits, fmt.channels);
                if (serr != ESP_OK) {
                    reason = (serr == ESP_ERR_NOT_SUPPORTED) ? PIPE_END_UNSUPPORTED
                                                             : PIPE_END_ERROR;
                    break;
                }
                sink_on = true;
            }
            /* CMD_RESUME while running, or a stray PLAY, is ignored (player serialises). */
        }

        if (off >= got) {                          /* previous chunk fully consumed: next one */
            if (decoder_read(s_file_buf, sizeof s_file_buf, &got) != ESP_OK) {
                reason = PIPE_END_ERROR; break;
            }
            if (got == 0) break;                   /* EOF */
            off = 0;
        }

        /* The sink may accept only part of the chunk (Bluetooth backpressure, or a stalled
           link). Advance by what it took and retry the rest next iteration -- dropping it
           instead would glitch, and the loop head keeps STOP/PAUSE responsive meanwhile. */
        size_t written = 0;
        sink->write(s_file_buf + off, got - off, &written);
        off += written;
        s_pos_frames += (uint32_t)(written / out_frame);
    }

    esp_task_wdt_delete(NULL);
    if (sink_on) sink->stop();
    decoder_close();
    s_pos_rate     = 0;                            /* nothing playing: position reads back 0/0 */
    s_pos_frames   = 0;
    s_pos_total_ms = 0;
    if (notify && s_end_cb) s_end_cb(reason);
}

static void audio_task(void *arg)
{
    (void)arg;
    pipe_cmd_t c;
    for (;;) {
        /* Idle: block until something asks to play. STOP/PAUSE/RESUME received here are no-ops. */
        if (xQueueReceive(s_cmd_q, &c, portMAX_DELAY)) {
            if (c.cmd == CMD_PLAY_TONE)      run_tone(c.freq);
            else if (c.cmd == CMD_PLAY_FILE) run_file(c.path);
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

esp_err_t pipeline_switch_sink(void)
{
    if (!s_cmd_q) return ESP_ERR_INVALID_STATE;
    pipe_cmd_t c = { .cmd = CMD_SWITCH_SINK };
    return xQueueSend(s_cmd_q, &c, 0) == pdPASS ? ESP_OK : ESP_FAIL;
}

void pipeline_set_track_end_cb(void (*cb)(pipeline_end_reason_t reason))
{
    s_end_cb = cb;
}

void pipeline_get_position(uint32_t *elapsed_ms, uint32_t *total_ms)
{
    uint32_t rate   = s_pos_rate;       /* read once: each is an atomic 32-bit load */
    uint32_t frames = s_pos_frames;
    if (elapsed_ms) *elapsed_ms = rate ? (uint32_t)((uint64_t)frames * 1000u / rate) : 0;
    if (total_ms)   *total_ms   = s_pos_total_ms;
}

esp_err_t pipeline_play_file(const char *path)
{
    if (!s_cmd_q) return ESP_ERR_INVALID_STATE;
    if (!path || strlen(path) >= PIPE_PATH_MAX) return ESP_ERR_INVALID_ARG;

    pipe_cmd_t c = { .cmd = CMD_PLAY_FILE };
    strcpy(c.path, path);
    return xQueueSend(s_cmd_q, &c, 0) == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t pipeline_pause(void)
{
    if (!s_cmd_q) return ESP_ERR_INVALID_STATE;
    pipe_cmd_t c = { .cmd = CMD_PAUSE };
    return xQueueSend(s_cmd_q, &c, 0) == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t pipeline_resume(void)
{
    if (!s_cmd_q) return ESP_ERR_INVALID_STATE;
    pipe_cmd_t c = { .cmd = CMD_RESUME };
    return xQueueSend(s_cmd_q, &c, 0) == pdPASS ? ESP_OK : ESP_FAIL;
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
