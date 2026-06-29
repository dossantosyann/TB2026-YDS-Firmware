/**
 * @file track_meta.c
 * @brief Tags + format reader (see track_meta.h): MP3 ID3v2/ID3v1, WAV header, no playback.
 * @ingroup services_audio_track_meta
 */
#include "track_meta.h"
#include "mp3_frame.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "track_meta";

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

/* ID3v2 sizes are "syncsafe": 7 usable bits per byte (the high bit is always 0). */
static uint32_t syncsafe(const uint8_t *p)
{
    return ((uint32_t)(p[0] & 0x7F) << 21) | ((uint32_t)(p[1] & 0x7F) << 14) |
           ((uint32_t)(p[2] & 0x7F) <<  7) |  (uint32_t)(p[3] & 0x7F);
}

/* File name without directory or extension: the title fallback when a track is untagged. */
static void basename_no_ext(const char *path, char *dst, size_t cap)
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    const char *dot = strrchr(base, '.');
    size_t len = dot ? (size_t)(dot - base) : strlen(base);
    if (len >= cap) len = cap - 1;
    memcpy(dst, base, len);
    dst[len] = '\0';
}

/* Copy a fixed-width, space/NUL-padded field (ID3v1) into a NUL-terminated string. */
static void copy_fixed(const uint8_t *src, size_t n, char *dst, size_t cap)
{
    size_t o = 0;
    for (size_t i = 0; i < n && o + 1 < cap; i++) {
        if (src[i] == 0) break;
        dst[o++] = (char)src[i];
    }
    dst[o] = '\0';
    while (o > 0 && dst[o - 1] == ' ') dst[--o] = '\0';
}

/* Decode an ID3v2 text frame body (first byte = encoding) into @p dst. Minimal: ISO-8859-1 and
   UTF-8 ride through; UTF-16 is reduced to its ASCII code points (enough for display/sort). */
static void copy_text(const uint8_t *buf, size_t n, char *dst, size_t cap)
{
    if (n == 0 || cap == 0) return;
    uint8_t enc = buf[0];
    const uint8_t *t = buf + 1;
    size_t tn = n - 1, o = 0;
    if (enc == 1 || enc == 2) {                 /* UTF-16: keep ASCII, drop BOM/zero/high bytes */
        for (size_t i = 0; i < tn && o + 1 < cap; i++) {
            if (t[i] >= 0x20 && t[i] <= 0x7E) dst[o++] = (char)t[i];
        }
    } else {                                    /* ISO-8859-1 (0) or UTF-8 (3) */
        for (size_t i = 0; i < tn && o + 1 < cap; i++) {
            if (t[i] == 0) break;
            dst[o++] = (char)t[i];
        }
    }
    dst[o] = '\0';
    while (o > 0 && dst[o - 1] == ' ') dst[--o] = '\0';
}

/* Parse ID3v2 frames starting at @p start; fills empty fields only.
   Returns the end offset of the tag (= audio start for MP3), or @p start if no tag. */
static long parse_id3v2_at(FILE *f, long start, track_meta_t *m)
{
    uint8_t h[10];
    if (fseek(f, start, SEEK_SET) != 0 || fread(h, 1, 10, f) != 10 || memcmp(h, "ID3", 3) != 0)
        return start;

    uint8_t ver = h[3];
    long total = start + 10 + (long)syncsafe(h + 6);
    if (ver != 3 && ver != 4) return total;     /* skip v2.2 frames; tag size still honoured */

    for (long pos = start + 10; pos + 10 <= total; ) {
        uint8_t fh[10];
        if (fseek(f, pos, SEEK_SET) != 0 || fread(fh, 1, 10, f) != 10) break;  /* truncated */
        if (fh[0] == 0) break;                  /* padding: no more frames */

        uint32_t fsize = (ver == 4) ? syncsafe(fh + 4) : be32(fh + 4);
        if (fsize == 0 || pos + 10 + (long)fsize > total) break;   /* garbage / overruns tag */

        char *dst = !memcmp(fh, "TIT2", 4) ? m->title
                  : !memcmp(fh, "TPE1", 4) ? m->artist
                  : !memcmp(fh, "TALB", 4) ? m->album : NULL;
        if (dst && dst[0] == '\0') {
            uint8_t tmp[128];
            size_t want = fsize < sizeof tmp ? fsize : sizeof tmp;
            copy_text(tmp, fread(tmp, 1, want, f), dst, TRACK_META_STR_MAX);
        }
        pos += 10 + (long)fsize;
    }
    return total;
}

/* Wrapper for the MP3 path: reads at offset 0, returns audio start offset. */
static long parse_id3v2(FILE *f, track_meta_t *m)
{
    return parse_id3v2_at(f, 0, m);
}

/* Parse WAV LIST/INFO sub-chunks (@p remaining = bytes left after the "INFO" type word).
   Fills title (INAM), artist (IART), album (IPRD) if currently empty. */
static void parse_list_info(FILE *f, uint32_t remaining, track_meta_t *m)
{
    while (remaining >= 8) {
        uint8_t sub[8];
        if (fread(sub, 1, 8, f) != 8) break;
        uint32_t ssize  = le32(sub + 4);
        uint32_t stride = ssize + (ssize & 1);
        if (8 + stride > remaining) break;
        remaining -= 8 + stride;

        char *dst = !memcmp(sub, "INAM", 4) ? (m->title[0]  ? NULL : m->title)
                  : !memcmp(sub, "IART", 4) ? (m->artist[0] ? NULL : m->artist)
                  : !memcmp(sub, "IPRD", 4) ? (m->album[0]  ? NULL : m->album)
                  : NULL;

        if (dst && ssize > 0) {
            uint32_t to_read = ssize < TRACK_META_STR_MAX - 1 ? ssize : TRACK_META_STR_MAX - 1;
            size_t got = fread(dst, 1, to_read, f);
            dst[got] = '\0';
            while (got > 0 && (unsigned char)dst[got - 1] <= ' ') dst[--got] = '\0';
            fseek(f, (long)(stride - to_read), SEEK_CUR);
        } else {
            fseek(f, (long)stride, SEEK_CUR);
        }
    }
}

/* ID3v1 (last 128 bytes): fills only the fields the ID3v2 pass left empty. */
static void parse_id3v1(FILE *f, track_meta_t *m)
{
    uint8_t t[128];
    if (fseek(f, -128, SEEK_END) != 0 || fread(t, 1, 128, f) != 128) return;
    if (memcmp(t, "TAG", 3) != 0) return;
    if (m->title[0]  == '\0') copy_fixed(t + 3,  30, m->title,  TRACK_META_STR_MAX);
    if (m->artist[0] == '\0') copy_fixed(t + 33, 30, m->artist, TRACK_META_STR_MAX);
    if (m->album[0]  == '\0') copy_fixed(t + 63, 30, m->album,  TRACK_META_STR_MAX);
}

/* Read rate/bitrate/duration from the first audio frame at @p audio_off (after any ID3v2). */
static esp_err_t read_mp3(FILE *f, track_meta_t *m, long audio_off)
{
    uint8_t buf[2048];
    if (fseek(f, audio_off, SEEK_SET) != 0) return ESP_FAIL;
    size_t n = fread(buf, 1, sizeof buf, f);

    mp3_frame_hdr_t fh;
    for (size_t i = 0; i + 4 <= n; i++) {
        if (buf[i] == 0xFF && mp3_frame_parse(buf + i, n - i, &fh)) {
            m->rate_hz     = fh.rate_hz;
            m->bits        = 16;
            m->bitrate_bps = fh.bitrate_bps;
            m->duration_ms = mp3_xing_duration_ms(buf + i, n - i, &fh);
            return ESP_OK;
        }
    }
    return ESP_FAIL;                             /* no decodable frame: fail loud */
}

/* WAV: rate/bits from the fmt chunk, exact duration from the data chunk size. No tags. */
static esp_err_t read_wav(FILE *f, track_meta_t *m)
{
    uint8_t hdr[12];
    rewind(f);
    if (fread(hdr, 1, 12, f) != 12) return ESP_FAIL;
    if (memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) return ESP_FAIL;

    uint16_t channels = 0, bits = 0;
    uint32_t rate = 0, data_bytes = 0;
    bool have_fmt = false, have_data = false;
    for (;;) {
        uint8_t ck[8];
        if (fread(ck, 1, 8, f) != 8) break;     /* ran out of chunks */
        uint32_t csize = le32(ck + 4);
        if (!memcmp(ck, "fmt ", 4)) {
            uint8_t b[16];
            if (csize < 16 || fread(b, 1, 16, f) != 16) return ESP_FAIL;
            channels = (uint16_t)(b[2] | b[3] << 8);
            rate     = le32(b + 4);
            bits     = (uint16_t)(b[14] | b[15] << 8);
            have_fmt = true;
            fseek(f, (long)(csize - 16) + (csize & 1), SEEK_CUR);
        } else if (!memcmp(ck, "data", 4)) {
            data_bytes = csize;
            have_data = true;
            break;
        } else if (!memcmp(ck, "LIST", 4) && csize >= 4) {
            long chunk_end = ftell(f) + (long)csize + (csize & 1);
            uint8_t sub[4];
            if (fread(sub, 1, 4, f) == 4 && !memcmp(sub, "INFO", 4))
                parse_list_info(f, csize - 4, m);
            fseek(f, chunk_end, SEEK_SET);
        } else if ((!memcmp(ck, "id3 ", 4) || !memcmp(ck, "ID3 ", 4)) && csize > 0) {
            long chunk_start = ftell(f);
            parse_id3v2_at(f, chunk_start, m);
            fseek(f, chunk_start + (long)csize + (csize & 1), SEEK_SET);
        } else {
            fseek(f, (long)csize + (csize & 1), SEEK_CUR);
        }
    }
    if (!have_fmt) return ESP_FAIL;

    m->rate_hz     = rate;
    m->bits        = (uint8_t)bits;
    m->bitrate_bps = 0;
    uint32_t byte_rate = rate * channels * (bits / 8);
    m->duration_ms = (have_data && byte_rate)
                         ? (uint32_t)((uint64_t)data_bytes * 1000u / byte_rate) : 0;
    return ESP_OK;
}

esp_err_t track_meta_read(const char *path, track_meta_t *out)
{
    if (!path || !out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof *out);

    FILE *f = fopen(path, "rb");
    if (!f) {
        basename_no_ext(path, out->title, TRACK_META_STR_MAX);
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t magic[12] = {0};
    size_t n = fread(magic, 1, sizeof magic, f);
    bool wav = n >= 12 && !memcmp(magic, "RIFF", 4) && !memcmp(magic + 8, "WAVE", 4);
    bool mp3 = n >= 3 && (!memcmp(magic, "ID3", 3) ||
                          (magic[0] == 0xFF && (magic[1] & 0xE0) == 0xE0));

    esp_err_t err;
    if (wav) {
        err = read_wav(f, out);
    } else if (mp3) {
        long audio_off = parse_id3v2(f, out);    /* tags + offset of the first audio frame */
        err = read_mp3(f, out, audio_off);
        if (err == ESP_OK) parse_id3v1(f, out);  /* fill any field the v2 tag lacked */
    } else {
        err = ESP_ERR_NOT_SUPPORTED;
    }
    fclose(f);

    if (out->title[0] == '\0')
        basename_no_ext(path, out->title, TRACK_META_STR_MAX);

    if (err != ESP_OK) ESP_LOGW(TAG, "meta read failed for %s (%d)", path, err);
    return err;
}
