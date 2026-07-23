/**
 * @file track_meta.h
 * @brief Read a track's tags and format without playing it.
 *
 * A standalone reader in the @ref services layer: it opens a file, parses the tags (ID3v2 with
 * an ID3v1 fallback for MP3), the duration and the audio format, then closes — no decoder, no
 * sink, no playback. This is what the Now Playing screen calls for the current track and what
 * a track browser can call per file (lazily, on demand) to show or sort by artist and
 * album. Reading one track at a time keeps SD and RAM cost flat; a library-wide pre-scan, if a
 * browser ever needs to sort the whole list, is just this call in a loop with the result cached.
 *
 * The duration field matches the playing decoder exactly (same Xing/Info/VBRI logic): 0 means
 * unknown (e.g. a headerless VBR MP3), never a guess.
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>

/**
 * @defgroup services_audio_track_meta Track metadata
 * @ingroup services
 * @brief Read tags + format from a file without playing it (for now_playing and the browser).
 * @{
 */

/** @brief Longest tag string kept, including the terminating NUL (longer tags are truncated). */
#define TRACK_META_STR_MAX 64

/** @brief A track's tags and audio format, filled by track_meta_read(). */
typedef struct {
    char     title[TRACK_META_STR_MAX];   /**< Tag title, or the file name if untagged. */
    char     artist[TRACK_META_STR_MAX];  /**< Tag artist, or empty if absent. */
    char     album[TRACK_META_STR_MAX];   /**< Tag album, or empty if absent. */
    uint32_t duration_ms;                 /**< Track length in ms; 0 if unknown. */
    uint32_t rate_hz;                     /**< Sample rate in Hz. */
    uint8_t  bits;                        /**< Bits per sample: 16 (MP3, 16-bit WAV) or 24. */
    uint32_t bitrate_bps;                 /**< MP3 nominal bitrate in bits/s; 0 for WAV. */
} track_meta_t;

/**
 * @brief Read the tags and format of @p path.
 *
 * Title always falls back to the file name (without extension) when no tag is present; artist
 * and album are left empty when absent. A malformed or truncated tag is bounds-checked and
 * degraded to the file-name fallback (logged), never trusted as garbage and never a crash.
 *
 * @param path     POSIX path under the SD mount, e.g. "/sdcard/track.mp3".
 * @param[out] out Filled on success (zeroed first). Must not be NULL.
 * @return ESP_OK; ESP_ERR_INVALID_ARG if an argument is NULL; ESP_ERR_NOT_FOUND if the file
 *         cannot be opened; ESP_ERR_NOT_SUPPORTED if the format is unrecognised; ESP_FAIL if
 *         the header/first frame cannot be parsed.
 */
esp_err_t track_meta_read(const char *path, track_meta_t *out);

/** @} */
