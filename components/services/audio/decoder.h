/**
 * @file decoder.h
 * @brief Audio file decoder: turns an MP3 or WAV file into interleaved PCM frames.
 *
 * The mirror image of @ref services_audio_sink on the input side. The public API is a
 * concrete facade: decoder_open() auto-detects the format (by extension, confirmed by the
 * file's magic bytes) and routes to the matching backend (libhelix MP3 or raw WAV). The
 * caller never sees which backend is active; it just reads PCM until end of file.
 *
 * Output layout is what the ESP32 I2S peripheral expects for the reported width, ready to
 * hand straight to sink->write():
 *   - 16-bit: int16 samples, 2 bytes each (MP3, 16-bit WAV);
 *   - 24-bit: uint32 samples MSB-justified (valid in the high 24 bits), 4 bytes each (WAV).
 * Always interleaved stereo (mono sources are up-mixed L=R), so the wired sink's fixed
 * stereo slots stay full.
 *
 * One stream at a time: decoder_open() while a stream is already open returns an error.
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @defgroup services_audio_decoder Audio decoder
 * @ingroup services
 * @brief Decode an MP3 or WAV file into PCM for an @ref services_audio_sink.
 * @{
 */

/**
 * @brief Largest number of PCM bytes a single decoder_read() can emit.
 *
 * One MP3 frame is at most 1152 samples/channel; up-mixed to stereo that is 2304 samples,
 * and at 24-bit width each sample is 4 bytes. The pcm buffer passed to decoder_read() must
 * be at least this big.
 */
#define DECODER_READ_BUF_BYTES  (2304 * 4)

/** @brief PCM format of the open stream, read once after decoder_open(). */
typedef struct {
    uint32_t rate_hz;   /**< Sample rate in Hz (e.g. 44100, up to 192000 for WAV). */
    uint8_t  channels;  /**< Always 2: mono sources are up-mixed to stereo. */
    uint8_t  bits;      /**< Bits per sample: 16 (MP3, 16-bit WAV) or 24 (24-bit WAV). */
} decoder_format_t;

/**
 * @brief Open an audio file, pick the backend, and read its format.
 *
 * Detection is by file extension (.mp3 / .wav, case-insensitive), confirmed against the
 * file's magic bytes (ID3/0xFFEx for MP3, RIFF/WAVE for WAV).
 *
 * @param path  POSIX path under the SD mount, e.g. "/sdcard/track.mp3".
 * @return ESP_OK; ESP_ERR_INVALID_STATE if a stream is already open;
 *         ESP_ERR_NOT_FOUND if the file cannot be opened;
 *         ESP_ERR_NOT_SUPPORTED if the format is unrecognised; ESP_FAIL on decode error.
 */
esp_err_t decoder_open(const char *path);

/**
 * @brief Get the PCM format of the open stream.
 *
 * @param fmt  Filled with the stream format. Valid only between open and close.
 * @return ESP_OK; ESP_ERR_INVALID_STATE if no stream is open.
 */
esp_err_t decoder_format(decoder_format_t *fmt);

/**
 * @brief Decode the next chunk of audio into @p pcm.
 *
 * @param pcm        Output buffer; must be at least @ref DECODER_READ_BUF_BYTES.
 * @param cap_bytes  Capacity of @p pcm in bytes.
 * @param got_bytes  Receives the number of bytes written; 0 means end of file.
 * @return ESP_OK; ESP_ERR_INVALID_STATE if no stream is open;
 *         ESP_ERR_INVALID_SIZE if @p cap_bytes is below @ref DECODER_READ_BUF_BYTES;
 *         ESP_FAIL on decode error.
 */
esp_err_t decoder_read(void *pcm, size_t cap_bytes, size_t *got_bytes);

/**
 * @brief Close the stream and free the decoder. Safe to call when nothing is open.
 *
 * @return ESP_OK.
 */
esp_err_t decoder_close(void);

/** @} */
