/**
 * @file sink_bluetooth.c
 * @brief Bluetooth A2DP audio sink: bridge the push pipeline to the pull A2DP source.
 * @ingroup services_audio_sink
 *
 * The pipeline pushes PCM via write(); the A2DP source pulls PCM from its own task. A stream
 * buffer decouples the two: write() blocks when it is full, which paces the pipeline (the
 * Bluetooth link is the real-time clock). The stack encodes SBC and paces transmission.
 */
#include "sink_bluetooth.h"
#include "bluetooth.h"

#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "esp_attr.h"

#include <string.h>

/* ~120 ms of 44.1 kHz / 16-bit / stereo PCM. The +1 byte is StreamBuffer's reserved slot.
   Storage lives in PSRAM (CPU-only access, no DMA) to keep internal DRAM free for the BT
   stack; the small control block stays in internal RAM. */
#define SINK_BT_RB_BYTES   (21 * 1024)
#define SINK_BT_WRITE_TO_MS 100

static StaticStreamBuffer_t s_rb_ctrl;
EXT_RAM_BSS_ATTR static uint8_t s_rb_store[SINK_BT_RB_BYTES + 1];
static StreamBufferHandle_t s_rb;

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

static esp_err_t bt_start(uint32_t rate_hz, uint8_t bits, uint8_t channels)
{
    (void)rate_hz; (void)bits; (void)channels;   /* the A2DP stack owns the negotiated format */
    if (!s_rb) {
        s_rb = xStreamBufferCreateStatic(SINK_BT_RB_BYTES, 1, s_rb_store, &s_rb_ctrl);
        if (!s_rb) return ESP_ERR_NO_MEM;
    } else {
        xStreamBufferReset(s_rb);
    }
    return bluetooth_audio_start(bt_pcm_cb);
}

static esp_err_t bt_write(const void *pcm, size_t len, size_t *written)
{
    size_t n = xStreamBufferSend(s_rb, pcm, len, pdMS_TO_TICKS(SINK_BT_WRITE_TO_MS));
    if (written) *written = n;
    return ESP_OK;
}

static esp_err_t bt_stop(void)
{
    return bluetooth_audio_stop();
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
