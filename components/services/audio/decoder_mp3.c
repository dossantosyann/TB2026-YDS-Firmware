/**
 * @file decoder_mp3.c
 * @brief MP3 backend: libhelix (fixed-point) decoding one frame per decoder_read().
 *
 * Owns a sliding compressed-input buffer: helix consumes from it and we top it up from the
 * file. Mono frames are up-mixed to stereo so the wired sink's fixed stereo slots stay full.
 */
#include "decoder_backend.h"
#include "mp3_frame.h"
#include "mp3dec.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

#define MP3_IN_BUF_SIZE  4096   /* comfortably larger than one 320 kbps frame */

static const char *TAG = "dec_mp3";

static FILE        *s_f;
static HMP3Decoder  s_dec;
static uint8_t     *s_in;        /* malloc'd input buffer */
static int          s_in_len;    /* unconsumed bytes available from s_read_ptr */
static uint8_t     *s_read_ptr;  /* helix read cursor within s_in */

/* Move the unconsumed bytes to the front and top up from the file.
   Returns the number of new bytes read (0 = end of file). */
static int mp3_refill(void)
{
    if (s_read_ptr != s_in && s_in_len > 0) {
        memmove(s_in, s_read_ptr, s_in_len);
    }
    s_read_ptr = s_in;
    size_t n = fread(s_in + s_in_len, 1, MP3_IN_BUF_SIZE - s_in_len, s_f);
    s_in_len += (int)n;
    return (int)n;
}

static void mp3_close(void)
{
    if (s_dec) { MP3FreeDecoder(s_dec); s_dec = NULL; }
    free(s_in);
    s_in = NULL;
    s_read_ptr = NULL;
    s_in_len = 0;
    s_f = NULL;
}

static esp_err_t mp3_open(FILE *f, decoder_format_t *fmt)
{
    s_f = f;
    s_in_len = 0;
    s_in = malloc(MP3_IN_BUF_SIZE);
    s_dec = s_in ? MP3InitDecoder() : NULL;
    if (!s_in || !s_dec) { mp3_close(); return ESP_ERR_NO_MEM; }
    s_read_ptr = s_in;

    /* Skip anything before the first frame sync (ID3 tag, junk). */
    mp3_refill();
    int off;
    while ((off = MP3FindSyncWord(s_read_ptr, s_in_len)) < 0) {
        s_in_len = 0;                       /* nothing useful here */
        if (mp3_refill() == 0) { mp3_close(); return ESP_FAIL; }   /* no frame in file */
    }
    s_read_ptr += off;
    s_in_len   -= off;

    MP3FrameInfo fi;
    if (MP3GetNextFrameInfo(s_dec, &fi, s_read_ptr) != ERR_MP3_NONE) {
        mp3_close();
        return ESP_FAIL;
    }
    fmt->rate_hz  = (uint32_t)fi.samprate;
    fmt->channels = 2;      /* we always emit stereo */
    fmt->bits     = 16;     /* helix outputs int16 */
    /* Duration from a Xing/Info/VBRI tag in this first frame; 0 (unknown) if none. */
    mp3_frame_hdr_t fh;
    fmt->duration_ms = mp3_frame_parse(s_read_ptr, (size_t)s_in_len, &fh)
                           ? mp3_xing_duration_ms(s_read_ptr, (size_t)s_in_len, &fh) : 0;
    return ESP_OK;
}

static esp_err_t mp3_read(void *pcm, size_t cap_bytes, size_t *got_bytes)
{
    *got_bytes = 0;
    if (cap_bytes < DECODER_READ_BUF_BYTES) return ESP_ERR_INVALID_SIZE;  /* one frame may not fit */
    if (!s_dec) return ESP_ERR_INVALID_STATE;
    short *out = (short *)pcm;

    for (;;) {
        if (s_in_len < MAINBUF_SIZE) mp3_refill();   /* keep a whole frame buffered */
        if (s_in_len == 0) return ESP_OK;            /* end of file */

        int off = MP3FindSyncWord(s_read_ptr, s_in_len);
        if (off < 0) {
            /* No sync in the buffer: keep the last byte (a split 0xFF) and refill. */
            if (s_in_len > 0) { s_in[0] = s_read_ptr[s_in_len - 1]; s_in_len = 1; }
            s_read_ptr = s_in;
            if (mp3_refill() == 0) return ESP_OK;    /* trailing junk at EOF */
            continue;
        }
        s_read_ptr += off;
        s_in_len   -= off;

        int err = MP3Decode(s_dec, &s_read_ptr, &s_in_len, out, 0);
        if (err == ERR_MP3_INDATA_UNDERFLOW || err == ERR_MP3_MAINDATA_UNDERFLOW) {
            if (mp3_refill() == 0) return ESP_OK;    /* can't complete the last frame */
            continue;
        }
        if (err != ERR_MP3_NONE) {
            ESP_LOGW(TAG, "decode error %d, resyncing", err);
            continue;                                /* skip frame, hunt the next sync */
        }

        MP3FrameInfo fi;
        MP3GetLastFrameInfo(s_dec, &fi);
        int samples = fi.outputSamps;                /* total across channels */
        if (fi.nChans == 1) {                        /* up-mix mono -> stereo, in place */
            for (int i = samples - 1; i >= 0; i--) { out[2 * i] = out[2 * i + 1] = out[i]; }
            samples *= 2;
        }
        /* Expand int16 -> int32 MSB-justified in place (buffer = 9216 B = 2304 × 4 B).
           Go from the end so the write destination never clobbers an unread source. */
        int32_t *out32 = (int32_t *)pcm;
        for (int i = samples - 1; i >= 0; i--) {
            out32[i] = (int32_t)out[i] << 16;
        }
        *got_bytes = (size_t)samples * sizeof(int32_t);
        return ESP_OK;
    }
}

const decoder_backend_t decoder_mp3_backend = {
    .open  = mp3_open,
    .read  = mp3_read,
    .close = mp3_close,
};
