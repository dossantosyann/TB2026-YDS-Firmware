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
