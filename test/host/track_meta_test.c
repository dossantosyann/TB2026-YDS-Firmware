/* Host unit test for the track-metadata reader (track_meta.c + mp3_frame.c).
   Builds tiny MP3 files (an ID3v2.3 tag and one MPEG-1 Layer III frame) and checks:
   tag present -> title/artist/album + format + Xing/Info duration; tag absent -> filename
   title and unknown duration; truncated tag -> fail loud, no crash. No ESP-IDF, no SD card. */
#include "track_meta.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* One MPEG-1 Layer III frame header (FF FB 90 00 = 128 kbps, 44.1 kHz, stereo) padded to 64 B.
   With @p with_info an "Info"/Xing tag carrying a 100-frame count is placed after the side
   info (offset 4 + 32), so duration = 100 * 1152 / 44100 = 2612 ms. */
static size_t put_mp3_frame(uint8_t *p, int with_info)
{
    memset(p, 0, 64);
    p[0] = 0xFF; p[1] = 0xFB; p[2] = 0x90; p[3] = 0x00;
    if (with_info) {
        memcpy(p + 36, "Info", 4);
        p[43] = 0x01;                  /* flags: frame-count present (big-endian 0x00000001) */
        p[47] = 100;                   /* frame count = 100 (big-endian 0x00000064) */
    }
    return 64;
}

/* Append an ID3v2.3 ISO-8859-1 text frame (plain big-endian size); returns bytes written. */
static size_t put_id3_frame(uint8_t *p, const char *id, const char *text)
{
    size_t tlen = strlen(text);
    uint32_t sz = (uint32_t)(1 + tlen);            /* encoding byte + text */
    memcpy(p, id, 4);
    p[4] = (uint8_t)(sz >> 24); p[5] = (uint8_t)(sz >> 16);
    p[6] = (uint8_t)(sz >> 8);  p[7] = (uint8_t)sz;
    p[8] = 0; p[9] = 0;                            /* frame flags */
    p[10] = 0x00;                                  /* encoding: ISO-8859-1 */
    memcpy(p + 11, text, tlen);
    return 11 + tlen;
}

/* ID3v2 10-byte header with a syncsafe body size. */
static size_t put_id3_header(uint8_t *p, uint32_t body)
{
    p[0]='I'; p[1]='D'; p[2]='3'; p[3]=3; p[4]=0; p[5]=0;
    p[6]=(uint8_t)((body>>21)&0x7F); p[7]=(uint8_t)((body>>14)&0x7F);
    p[8]=(uint8_t)((body>>7)&0x7F);  p[9]=(uint8_t)(body&0x7F);
    return 10;
}

static void write_file(const char *path, const uint8_t *buf, size_t n)
{
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fwrite(buf, 1, n, f) == n);
    fclose(f);
}

int main(void)
{
    char dir[] = "/tmp/tmeta_XXXXXX";
    assert(mkdtemp(dir));
    char path[256];
    uint8_t buf[512];
    track_meta_t m;
    size_t n;

    /* --- tag present: ID3v2.3 (title/artist/album) + a framed audio with an Info header --- */
    uint8_t body[256];
    size_t b = 0;
    b += put_id3_frame(body + b, "TIT2", "My Title");
    b += put_id3_frame(body + b, "TPE1", "My Artist");
    b += put_id3_frame(body + b, "TALB", "My Album");
    n  = put_id3_header(buf, (uint32_t)b);
    memcpy(buf + n, body, b); n += b;
    n += put_mp3_frame(buf + n, 1);
    snprintf(path, sizeof path, "%s/song.mp3", dir);
    write_file(path, buf, n);

    assert(track_meta_read(path, &m) == ESP_OK);
    assert(strcmp(m.title,  "My Title")  == 0);
    assert(strcmp(m.artist, "My Artist") == 0);
    assert(strcmp(m.album,  "My Album")  == 0);
    assert(m.rate_hz == 44100 && m.bits == 16 && m.bitrate_bps == 128000);
    assert(m.duration_ms == 2612);
    unlink(path);

    /* --- tag absent: bare frame, no ID3, no Xing -> filename title, unknown duration --- */
    n = put_mp3_frame(buf, 0);
    snprintf(path, sizeof path, "%s/Greatest Hit.mp3", dir);
    write_file(path, buf, n);

    assert(track_meta_read(path, &m) == ESP_OK);
    assert(strcmp(m.title, "Greatest Hit") == 0);
    assert(m.artist[0] == '\0' && m.album[0] == '\0');
    assert(m.rate_hz == 44100 && m.bitrate_bps == 128000 && m.duration_ms == 0);
    unlink(path);

    /* --- truncated tag: header claims a large tag the file doesn't contain -> fail loud --- */
    n = put_id3_header(buf, 16000);                /* ~16 kB body, none present */
    snprintf(path, sizeof path, "%s/broken.mp3", dir);
    write_file(path, buf, n);                       /* only the 10-byte header on disk */
    assert(track_meta_read(path, &m) == ESP_FAIL);  /* no decodable frame, no crash */
    unlink(path);

    rmdir(dir);
    printf("track_meta_test: OK\n");
    return 0;
}
