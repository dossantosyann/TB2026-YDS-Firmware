/**
 * @file sink_bluetooth.c
 * @brief Bluetooth A2DP audio sink: bridge the push pipeline to the pull A2DP source.
 * @ingroup services_audio_sink
 *
 * The pipeline pushes PCM via write(); the A2DP source pulls PCM from its own task. A stream
 * buffer decouples the two: write() paces the pipeline by accepting only what fits (the
 * Bluetooth link is the real-time clock). The stack encodes SBC and paces transmission.
 *
 * The media session outlives one start/stop pair: stop() only arms a deferred SUSPEND and
 * start() cancels it, so a track change (stop+start back-to-back) keeps the stream running.
 * The SUSPEND/CHECK_SRC_RDY/START media commands are asynchronous and unserialised in the
 * stack; issuing a pair of them per skip both added ~1 s of latency and could interleave so
 * the session ended up suspended while the player believed it was playing (silent tracks).
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "sink_bluetooth.h"
#include "bluetooth.h"
#include "decoder.h"

#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "esp_attr.h"

#include <string.h>
#include <stdint.h>

/* ~120 ms of 44.1 kHz / 16-bit / stereo PCM. The +1 byte is StreamBuffer's reserved slot.
   Storage lives in PSRAM (CPU-only access, no DMA) to keep internal DRAM free for the BT
   stack; the small control block stays in internal RAM. */
#define SINK_BT_RB_BYTES   (21 * 1024)
#define SINK_BT_WRITE_TO_MS 100          /* give up on a full ring after this (stalled link) */
#define SINK_BT_FRAME_BYTES 4            /* one 16-bit stereo frame in the ring */
#define SINK_BT_SUSPEND_DELAY_MS 3000    /* stop->SUSPEND grace: rides out skips and short pauses */

static StaticStreamBuffer_t s_rb_ctrl;
EXT_RAM_BSS_ATTR static uint8_t s_rb_store[SINK_BT_RB_BYTES + 1];
static StreamBufferHandle_t s_rb;

/* Media-session state, shared between the audio task (start/stop) and the timer task (the
   deferred SUSPEND). s_mutex makes the "is a suspend still wanted?" decision atomic, so a
   start() can never race the timer into suspending a session it just decided to keep. */
static SemaphoreHandle_t s_mutex;
static TimerHandle_t     s_susp_timer;
static bool              s_streaming;     /* A2DP media session is started */
static bool              s_susp_armed;    /* a stop() wants the session suspended */

/* A2DP source pull callback, run in the BT task: drain the ring buffer into @p buf. On
   underrun, zero-fill so the link keeps flowing (silence beats a stall). len == -1 is a
   flush request. Never blocks. */
static int32_t bt_pcm_cb(uint8_t *buf, int32_t len)
{
    if (len < 0) {
        if (s_rb) xStreamBufferReset(s_rb);
        return 0;
    }
    size_t got = s_rb ? xStreamBufferReceive(s_rb, buf, len, 0) : 0;
    if (got < (size_t)len) memset(buf + got, 0, (size_t)len - got);
    return len;
}

/* Deferred SUSPEND (timer task): only if no start() re-claimed the session since the stop()
   that armed it. A failure (link already gone, stack down) is fine: nothing left to suspend. */
static void susp_timer_cb(TimerHandle_t t)
{
    (void)t;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_susp_armed) {
        s_susp_armed = false;
        s_streaming  = false;
        bluetooth_audio_stop();
    }
    xSemaphoreGive(s_mutex);
}

static esp_err_t bt_start(uint32_t rate_hz, uint8_t bits, uint8_t channels)
{
    (void)bits; (void)channels;   /* always 32-bit MSB-justified stereo in, 16-bit stereo out */

    /* The A2DP endpoint is negotiated once per session at 44.1 kHz and there is no resampler
       in the pipeline: any other file rate would play at the wrong speed. Refuse it so the
       player can tell the user, instead of streaming garbage. (See the endpoint comment in
       bluetooth.c; lifting this means adding a resampler, then re-offering SF_48K there.) */
    if (rate_hz != 44100) return ESP_ERR_NOT_SUPPORTED;

    if (!s_rb) {
        s_rb = xStreamBufferCreateStatic(SINK_BT_RB_BYTES, 1, s_rb_store, &s_rb_ctrl);
        if (!s_rb) return ESP_ERR_NO_MEM;
    }
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) return ESP_ERR_NO_MEM;
    }
    if (!s_susp_timer) {
        s_susp_timer = xTimerCreate("bt_susp", pdMS_TO_TICKS(SINK_BT_SUSPEND_DELAY_MS),
                                    pdFALSE, NULL, susp_timer_cb);
        if (!s_susp_timer) return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_susp_armed = false;                          /* cancel any pending deferred SUSPEND */
    esp_err_t err = ESP_OK;
    if (s_streaming && bluetooth_is_connected()) {
        /* Session still live (track change / quick resume): keep it, and keep the ring's
           buffered tail so the previous track ends cleanly instead of being cut. */
    } else {
        xStreamBufferReset(s_rb);
        err = bluetooth_audio_start(bt_pcm_cb);
        s_streaming = (err == ESP_OK);
    }
    xSemaphoreGive(s_mutex);
    xTimerStop(s_susp_timer, 0);
    return err;
}

/* Down-convert scratch: the pipeline pushes 32-bit MSB-justified PCM, but the A2DP SBC encoder
   wants 16-bit. One decoder chunk (DECODER_READ_BUF_BYTES of int32) halves to this many int16.
   Only bt_write() (the audio task) touches it, so a static buffer is safe and keeps the small
   task stack free. */
static int16_t s_conv[DECODER_READ_BUF_BYTES / sizeof(int32_t)];

static esp_err_t bt_write(const void *pcm, size_t len, size_t *written)
{
    /* 32-bit MSB-justified stereo -> 16-bit stereo (keep the high half of each sample). */
    const int32_t *in = pcm;
    size_t samples = len / sizeof(int32_t);
    if (samples > (sizeof s_conv / sizeof s_conv[0])) samples = sizeof s_conv / sizeof s_conv[0];
    samples &= ~(size_t)1;                    /* whole L/R pairs only (decoder always sends pairs) */
    for (size_t i = 0; i < samples; i++) s_conv[i] = (int16_t)(in[i] >> 16);

    /* Feed the ring in whole frames that already fit, never with a blocking send: a send that
       times out mid-sample leaves the ring byte-shifted, and every sample after that decodes
       as white noise. Free space only grows (single reader), so a fitted send completes in
       full; when the ring is full, sleep a tick and retry, bounded so a stalled link cannot
       wedge the audio task (the unsent tail is reported back via *written and retried). */
    const size_t bytes = samples * sizeof(int16_t);
    size_t off = 0;
    uint32_t waited_ms = 0;
    while (off < bytes) {
        size_t space = xStreamBufferSpacesAvailable(s_rb) & ~(size_t)(SINK_BT_FRAME_BYTES - 1);
        if (space == 0) {
            if (waited_ms >= SINK_BT_WRITE_TO_MS) break;
            vTaskDelay(pdMS_TO_TICKS(10));
            waited_ms += 10;
            continue;
        }
        size_t n = (bytes - off < space) ? bytes - off : space;
        off += xStreamBufferSend(s_rb, (const uint8_t *)s_conv + off, n, 0);
        waited_ms = 0;
    }
    /* Report consumption in the caller's 32-bit domain: each 16-bit sample sent came from one. */
    if (written) *written = (off / sizeof(int16_t)) * sizeof(int32_t);
    return ESP_OK;
}

static esp_err_t bt_stop(void)
{
    /* Don't SUSPEND now: a track change stops and restarts the sink back-to-back, and pause is
       often short. Arm the deferred SUSPEND instead; bt_start() cancels it, the timer fires it
       for a real stop/long pause (the session then streams the ring tail + silence until then). */
    if (!s_susp_timer) return ESP_OK;         /* never started: nothing to suspend */
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_susp_armed = true;
    xSemaphoreGive(s_mutex);
    xTimerReset(s_susp_timer, 0);
    return ESP_OK;
}

static esp_err_t bt_set_volume(uint8_t left, uint8_t right)
{
    /* No-op on purpose. The speaker renders volume itself, set over AVRCP via
       bluetooth_set_absolute_volume() and driven by the pot. The pipeline's per-channel level is
       a DAC-domain (PCM attenuation) value; forwarding it here would override the safe,
       pot-driven speaker volume — including blasting the tone test's fixed level over a silent
       setting. So the BT sink does not take volume from the pipeline. */
    (void)left; (void)right;
    return ESP_OK;
}

const audio_sink_t *sink_bluetooth_get(void)
{
    static const audio_sink_t sink = {
        .start      = bt_start,
        .write      = bt_write,
        .stop       = bt_stop,
        .set_volume = bt_set_volume,
    };
    return &sink;
}
