#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @defgroup services_audio_sink Audio sink interface
 * @ingroup services
 * @brief Output-agnostic PCM sink: the pipeline writes frames here without knowing
 *        whether they go to the wired DAC or to Bluetooth A2DP.
 * @{
 */

/**
 * @brief A PCM output backend (wired I2S DAC or Bluetooth A2DP).
 *
 * The pipeline drives one of these at a time. start() configures the backend for a
 * track's format and powers the path up; write() streams interleaved PCM frames; stop()
 * powers it down; set_volume() applies per-channel levels.
 *
 * The contract is push-style (the caller drives write()). A2DP source is pull-based (the
 * BT stack asks for PCM via a callback), so that backend buffers internally to honour the
 * same interface; the pipeline stays identical for both outputs.
 */
typedef struct {
    /** Configure for rate_hz / bits-per-sample / channel count and power the path up. */
    esp_err_t (*start)(uint32_t rate_hz, uint8_t bits, uint8_t channels);
    /** Stream @p len bytes of interleaved PCM; @p *written receives the bytes accepted. */
    esp_err_t (*write)(const void *pcm, size_t len, size_t *written);
    /** Power the path down (pop-free where applicable). */
    esp_err_t (*stop)(void);
    /** Set per-channel volume; the byte scale is backend-specific. */
    esp_err_t (*set_volume)(uint8_t left, uint8_t right);
} audio_sink_t;

/** @} */
