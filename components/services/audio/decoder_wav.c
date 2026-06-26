/**
 * @file decoder_wav.c
 * @brief WAV backend: PCM RIFF/WAVE, 16-bit or 24-bit, mono or stereo, up to 192 kHz.
 *
 * 16-bit rides straight through as int16. 24-bit is expanded from WAV's packed 3-byte
 * little-endian samples to the 32-bit MSB-justified word the ESP32 I2S peripheral expects
 * (sample in the high 24 bits, low byte 0). Mono is up-mixed to stereo.
 */
#include "decoder_backend.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "dec_wav";

static FILE    *s_f;
static uint32_t s_data_remaining;   /* bytes left in the data chunk */
static uint8_t  s_channels;         /* 1 or 2 (source) */
static uint8_t  s_bits;             /* 16 or 24 */
static uint8_t *s_temp;             /* scratch for one read's worth of source bytes */

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void wav_close(void)
{
    free(s_temp);
    s_temp = NULL;
    s_f = NULL;
    s_data_remaining = 0;
}

static esp_err_t wav_open(FILE *f, decoder_format_t *fmt)
{
    s_f = f;
    s_temp = NULL;

    uint8_t hdr[12];
    if (fread(hdr, 1, 12, f) != 12) return ESP_FAIL;
    if (memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) return ESP_FAIL;

    uint16_t fmt_tag = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    bool have_fmt = false;

    for (;;) {
        uint8_t ck[8];
        if (fread(ck, 1, 8, f) != 8) return ESP_FAIL;   /* ran out before a data chunk */
        uint32_t csize = le32(ck + 4);

        if (!memcmp(ck, "fmt ", 4)) {
            uint8_t b[16];
            if (csize < 16 || fread(b, 1, 16, f) != 16) return ESP_FAIL;
            fmt_tag  = (uint16_t)(b[0]  | b[1]  << 8);
            channels = (uint16_t)(b[2]  | b[3]  << 8);
            rate     = le32(b + 4);
            bits     = (uint16_t)(b[14] | b[15] << 8);
            have_fmt = true;
            fseek(f, (long)(csize - 16) + (csize & 1), SEEK_CUR);  /* skip ext + pad byte */
        } else if (!memcmp(ck, "data", 4)) {
            if (!have_fmt) return ESP_FAIL;
            s_data_remaining = csize;
            break;                                       /* now positioned at PCM */
        } else {
            fseek(f, (long)csize + (csize & 1), SEEK_CUR);   /* chunks are word-aligned */
        }
    }

    if (fmt_tag != 1)                       return ESP_ERR_NOT_SUPPORTED;  /* PCM only */
    if (channels < 1 || channels > 2)       return ESP_ERR_NOT_SUPPORTED;
    if (bits != 16 && bits != 24)           return ESP_ERR_NOT_SUPPORTED;

    s_channels = (uint8_t)channels;
    s_bits     = (uint8_t)bits;
    s_temp = malloc(DECODER_READ_BUF_BYTES);
    if (!s_temp) { wav_close(); return ESP_ERR_NO_MEM; }

    fmt->rate_hz  = rate;
    fmt->channels = 2;
    fmt->bits     = (uint8_t)bits;
    ESP_LOGI(TAG, "WAV %u Hz, %u-bit, %u ch", (unsigned)rate, bits, channels);
    return ESP_OK;
}

static esp_err_t wav_read(void *pcm, size_t cap_bytes, size_t *got_bytes)
{
    *got_bytes = 0;
    if (!s_f) return ESP_ERR_INVALID_STATE;
    if (s_data_remaining == 0) return ESP_OK;            /* end of file */

    if (cap_bytes > DECODER_READ_BUF_BYTES) cap_bytes = DECODER_READ_BUF_BYTES;

    const size_t src_frame = (size_t)(s_bits / 8) * s_channels;  /* bytes per source frame */
    const size_t out_frame = (s_bits == 24) ? 8 : 4;            /* always stereo out */

    size_t max_frames = cap_bytes / out_frame;
    size_t avail      = s_data_remaining / src_frame;
    size_t frames     = max_frames < avail ? max_frames : avail;
    if (frames == 0) { s_data_remaining = 0; return ESP_OK; }   /* dangling partial frame */

    size_t n = fread(s_temp, 1, frames * src_frame, s_f);
    frames = n / src_frame;
    if (frames == 0) { s_data_remaining = 0; return ESP_OK; }
    s_data_remaining -= (uint32_t)(frames * src_frame);

    if (s_bits == 16) {
        const int16_t *in = (const int16_t *)s_temp;
        int16_t *out = (int16_t *)pcm;
        if (s_channels == 2) {
            memcpy(out, in, frames * 4);
        } else {
            for (size_t i = 0; i < frames; i++) { out[2 * i] = out[2 * i + 1] = in[i]; }
        }
    } else {  /* 24-bit: packed 3 bytes [b0,b1,b2] -> 32-bit word [0,b0,b1,b2] (high 24 bits) */
        const uint8_t *in = s_temp;
        uint8_t *out = (uint8_t *)pcm;
        for (size_t i = 0; i < frames; i++) {
            for (int ch = 0; ch < 2; ch++) {
                const uint8_t *s = in + (s_channels == 2 ? ch : 0) * 3;
                *out++ = 0;  *out++ = s[0];  *out++ = s[1];  *out++ = s[2];
            }
            in += src_frame;
        }
    }

    *got_bytes = frames * out_frame;
    return ESP_OK;
}

const decoder_backend_t decoder_wav_backend = {
    .open  = wav_open,
    .read  = wav_read,
    .close = wav_close,
};
