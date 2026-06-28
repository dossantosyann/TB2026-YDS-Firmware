/**
 * @file mp3_frame.h
 * @brief Internal MPEG-1/2 Layer III header + VBR-tag (Xing/Info/VBRI) parsing.
 *
 * Not a public header: shared by the MP3 decoder backend (decoder_mp3.c, for the playing
 * stream's duration) and the track-metadata reader (track_meta.c, for bitrate + duration).
 * Pure byte math with no ESP-IDF or libhelix dependency, so the duration logic lives in one
 * place and is host-testable on its own.
 *
 * @ingroup services_audio_decoder
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** @brief Decoded fields of one MPEG Layer III frame header. */
typedef struct {
    uint32_t rate_hz;            /**< Sample rate in Hz. */
    uint32_t bitrate_bps;        /**< Nominal bitrate of this frame in bits/s (0 = "free"). */
    uint16_t samples_per_frame;  /**< PCM samples per channel in this frame (576 or 1152). */
    uint8_t  channels;           /**< 1 (mono) or 2. */
    uint8_t  side_info_bytes;    /**< Side-information size = offset of a Xing/Info tag. */
} mp3_frame_hdr_t;

/**
 * @brief Parse a 4-byte MPEG Layer III frame header at @p p.
 *
 * @param p    Candidate frame start (the 0xFF sync byte).
 * @param len  Bytes readable from @p p (must be >= 4).
 * @param out  Filled on success.
 * @return true if @p p holds a valid Layer III header; false otherwise.
 */
bool mp3_frame_parse(const uint8_t *p, size_t len, mp3_frame_hdr_t *out);

/**
 * @brief Track duration from a Xing/Info/VBRI tag in the first frame, if present.
 *
 * @param frame  First frame start (its 0xFF sync byte).
 * @param len    Bytes readable from @p frame.
 * @param hdr    Header already parsed by mp3_frame_parse() for @p frame.
 * @return Duration in ms, or 0 when no usable VBR tag is present (duration unknown).
 */
uint32_t mp3_xing_duration_ms(const uint8_t *frame, size_t len, const mp3_frame_hdr_t *hdr);
