/**
 * @file sink_bluetooth.h
 * @brief Bluetooth A2DP audio sink: stream PCM to a connected speaker.
 *
 * Adapts the push-style @ref services_audio_sink interface to the A2DP source, which pulls
 * PCM from the Bluetooth stack's own task. write() feeds an internal ring buffer; a pull
 * callback drains it and the stack encodes SBC and paces the link. Requires the
 * @ref services_bluetooth service to have an established connection.
 */
#pragma once

#include "audio_sink.h"

/**
 * @brief Get the Bluetooth A2DP sink (PCM -> ring buffer -> A2DP source -> speaker).
 *
 * @return Pointer to the static sink vtable (never NULL). start() requires a live Bluetooth
 *         connection (see bluetooth_connect()).
 */
const audio_sink_t *sink_bluetooth_get(void);
