/**
 * @file mp3_frame.c
 * @brief MPEG Layer III header + Xing/Info/VBRI parsing (see mp3_frame.h).
 * @ingroup services_audio_decoder
 */
#include "mp3_frame.h"
#include <string.h>

/* Layer III bitrate tables (kbps), indexed by the 4-bit field; 0 = "free"/invalid. */
static const uint16_t k_bitrate_mpeg1[16] = {
    0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 };
static const uint16_t k_bitrate_mpeg2[16] = {
    0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 };

static const uint32_t k_rate_mpeg1[4]  = { 44100, 48000, 32000, 0 };
static const uint32_t k_rate_mpeg2[4]  = { 22050, 24000, 16000, 0 };
static const uint32_t k_rate_mpeg25[4] = { 11025, 12000,  8000, 0 };

bool mp3_frame_parse(const uint8_t *p, size_t len, mp3_frame_hdr_t *out)
{
    if (len < 4) return false;
    if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return false;     /* frame sync */

    uint8_t ver_bits   = (p[1] >> 3) & 0x03;   /* 00=2.5, 10=2, 11=1 (01 reserved) */
    uint8_t layer_bits = (p[1] >> 1) & 0x03;   /* 01 = Layer III */
    if (ver_bits == 0x01 || layer_bits != 0x01) return false;

    uint8_t br_idx = (p[2] >> 4) & 0x0F;
    uint8_t sr_idx = (p[2] >> 2) & 0x03;
    uint8_t mode   = (p[3] >> 6) & 0x03;       /* 11 = mono */
    if (br_idx == 0x0F || sr_idx == 0x03) return false;          /* invalid fields */

    bool mpeg1 = (ver_bits == 0x03);
    uint32_t rate = mpeg1 ? k_rate_mpeg1[sr_idx]
                  : (ver_bits == 0x02 ? k_rate_mpeg2[sr_idx] : k_rate_mpeg25[sr_idx]);
    if (rate == 0) return false;

    out->rate_hz           = rate;
    out->bitrate_bps       = (uint32_t)(mpeg1 ? k_bitrate_mpeg1[br_idx]
                                              : k_bitrate_mpeg2[br_idx]) * 1000u;
    out->channels          = (mode == 0x03) ? 1 : 2;
    out->samples_per_frame = mpeg1 ? 1152 : 576;
    out->side_info_bytes   = mpeg1 ? (out->channels == 1 ? 17 : 32)
                                   : (out->channels == 1 ?  9 : 17);
    return true;
}

static uint32_t be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

uint32_t mp3_xing_duration_ms(const uint8_t *frame, size_t len, const mp3_frame_hdr_t *hdr)
{
    uint32_t frames = 0;

    /* Xing (VBR) / Info (CBR) sit right after the 4-byte header + side-info block. */
    size_t xoff = 4u + hdr->side_info_bytes;
    if (xoff + 12 <= len &&
        (!memcmp(frame + xoff, "Xing", 4) || !memcmp(frame + xoff, "Info", 4))) {
        uint32_t flags = be32(frame + xoff + 4);
        if (flags & 0x0001) frames = be32(frame + xoff + 8);     /* frame-count present */
    } else if (4u + 32u + 18u <= len && !memcmp(frame + 4 + 32, "VBRI", 4)) {
        frames = be32(frame + 4 + 32 + 14);                      /* VBRI: count at +14 */
    }

    if (frames == 0) return 0;                                   /* no VBR tag: unknown */
    return (uint32_t)((uint64_t)frames * hdr->samples_per_frame * 1000u / hdr->rate_hz);
}
