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

    mp3_refill();

    /* Skip an ID3v2 tag by its declared size. Its payload (embedded album art is
       full of 0xFF bytes) would otherwise trip MP3FindSyncWord on a false sync.
       The tag can dwarf the input buffer, so seek past it in the file. Size is
       four synchsafe bytes (7 bits each); a footer flag adds another 10 bytes. */
    if (s_in_len >= 10 && !memcmp(s_in, "ID3", 3)) {
        const uint8_t *h = s_in;
        long tag = 10 + (((long)(h[6] & 0x7F) << 21) | ((long)(h[7] & 0x7F) << 14) |
                         ((long)(h[8] & 0x7F) << 7)  |  (long)(h[9] & 0x7F));
        if (h[5] & 0x10) tag += 10;              /* footer present */
        fseek(s_f, tag, SEEK_SET);
        s_read_ptr = s_in;
        s_in_len = 0;
        mp3_refill();
    }

    /* Find the first real frame. A 0xFF 0xEx pattern can still occur in trailing
       junk, so confirm each candidate sync with MP3GetNextFrameInfo and step past
       the false ones rather than trusting the first hit. */
    MP3FrameInfo fi;
    for (;;) {
        int off = MP3FindSyncWord(s_read_ptr, s_in_len);
        if (off >= 0 && s_in_len - off >= 4) {
            s_read_ptr += off;
            s_in_len   -= off;
            if (MP3GetNextFrameInfo(s_dec, &fi, s_read_ptr) == ERR_MP3_NONE) break;
            s_read_ptr += 1;                     /* false sync: step over the 0xFF */
            s_in_len   -= 1;
            continue;
        }
        if (off < 0) { s_in_len = 0; s_read_ptr = s_in; }  /* whole buffer was junk */
        if (mp3_refill() == 0) {
            ESP_LOGE(TAG, "no valid MP3 frame found (empty or not an MP3 file)");
            mp3_close(); return ESP_FAIL;
        }
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
