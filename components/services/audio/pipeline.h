/**
 * @file pipeline.h
 * @brief Audio pipeline: the real-time task that pulls PCM from a source and pushes it to
 *        an @ref services_audio_sink.
 *
 * The pipeline owns one high-priority FreeRTOS task pinned to the application core (kept
 * off core 0, where the Wi-Fi/BT stack runs). The task drives a producer->consumer loop:
 * fill a buffer from the current source, hand it to sink->write(), repeat. Only the fill
 * step depends on the source: a synthetic sine (bring-up/diagnostic, below) or, once track
 * playback lands, the decoder. Everything else (task, buffering, sink sequencing, volume)
 * is shared, so the tone exercises the real pipeline without needing the SD card.
 *
 * Commands are posted from any task and serviced by the audio task; the public calls below
 * never block on the audio path.
 */
#pragma once

#include "esp_err.h"
#include "audio_sink.h"
#include <stdint.h>

/**
 * @defgroup services_audio_pipeline Audio pipeline
 * @ingroup services
 * @brief Real-time task feeding PCM from a source to an @ref services_audio_sink.
 * @{
 */

/** @brief Why a file playback ended, reported to the track-end callback. */
typedef enum {
    PIPE_END_EOF,    /**< The decoder reached end of file: play the next track. */
    PIPE_END_ERROR,  /**< Open or decode failed: stop and surface the error. */
} pipeline_end_reason_t;

/**
 * @brief Create the pinned audio task and its command queue.
 *
 * Call once at startup, after sink_i2s_dac_init() (the pipeline drives the wired sink but
 * does not own the I2S bus). Idempotent.
 *
 * @return ESP_OK; ESP_ERR_NO_MEM if the queue or task cannot be created.
 */
esp_err_t pipeline_init(void);

/**
 * @brief Register the callback fired when a file playback ends on its own (EOF or error).
 *
 * The transport service (player) registers this to chain to the next track. The callback
 * runs in the audio task's context right after the stream closes, so it must not block; the
 * intended body just picks the next track and calls pipeline_play_file() (which only enqueues
 * a command). A user-initiated pipeline_stop() does NOT fire it.
 *
 * @param cb  Callback, or NULL to clear.
 */
void pipeline_set_track_end_cb(void (*cb)(pipeline_end_reason_t reason));

/**
 * @brief Play an audio file: decode it to the current sink at the file's own format.
 *
 * Opens @p path, reads its real PCM format (44.1k/16, 48k/24, ...) and starts the sink for
 * it, then streams until end of file, pipeline_stop(), or a decode error. The pipeline does
 * NOT set the volume here: the volume service owns the pot->output mapping during playback.
 * On natural end (EOF or error) the track-end callback fires.
 *
 * @param path  POSIX path under the SD mount, e.g. "/sdcard/track.mp3".
 * @return ESP_OK; ESP_ERR_INVALID_STATE if pipeline_init() has not run;
 *         ESP_ERR_INVALID_ARG if @p path is NULL or too long; ESP_FAIL if the queue is full.
 */
esp_err_t pipeline_play_file(const char *path);

/**
 * @brief Pause file playback: stop feeding the sink while keeping the stream position.
 *
 * Powers the output path down (no I2S underrun, minimal draw) and leaves the decoder open at
 * its current position; the audio task sleeps until pipeline_resume() or pipeline_stop().
 * No effect on the diagnostic tone.
 *
 * @return ESP_OK; ESP_ERR_INVALID_STATE if pipeline_init() has not run; ESP_FAIL if full.
 */
esp_err_t pipeline_pause(void);

/**
 * @brief Resume a playback paused with pipeline_pause(): restart the sink and keep decoding.
 *
 * @return ESP_OK; ESP_ERR_INVALID_STATE if pipeline_init() has not run; ESP_FAIL if full.
 */
esp_err_t pipeline_resume(void);

/**
 * @brief Select the output backend used by the next playback.
 *
 * Defaults to the wired DAC (set in pipeline_init). Pass sink_bluetooth_get() to route audio
 * to a connected speaker, or sink_i2s_dac_get() for the jack. Takes effect on the next
 * playback; changing it mid-playback does not interrupt the current tone.
 *
 * @param sink  Sink vtable to use; ignored if NULL.
 */
void pipeline_set_sink(const audio_sink_t *sink);

/**
 * @brief Bring-up/diagnostic: stream a continuous sine to the current sink.
 *
 * Plays until pipeline_stop(). Not part of normal playback; it validates the real-time
 * path (task scheduling, buffering, sink start/write/stop sequencing) with a synthetic
 * source while no file source is available.
 *
 * @param freq_hz  Tone frequency, 20..PIPE_FS/2 Hz. The exact tone is rounded to a whole
 *                 number of samples per period so buffers loop seamlessly.
 * @return ESP_OK; ESP_ERR_INVALID_STATE if pipeline_init() has not run;
 *         ESP_ERR_INVALID_ARG if @p freq_hz is out of range; ESP_FAIL if the queue is full.
 */
esp_err_t pipeline_play_tone(uint32_t freq_hz);

/**
 * @brief Stop playback and power the output path down. Safe to call when already idle.
 *
 * @return ESP_OK; ESP_ERR_INVALID_STATE if pipeline_init() has not run;
 *         ESP_FAIL if the queue is full.
 */
esp_err_t pipeline_stop(void);

/** @} */
