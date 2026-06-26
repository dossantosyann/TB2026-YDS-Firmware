/**
 * @file bluetooth.h
 * @brief Classic Bluetooth A2DP source: bring up the stack and connect to a speaker.
 *
 * Owns the Bluedroid stack and the A2DP source role: it powers the radio up, advertises an
 * SBC stream endpoint, discovers nearby devices and connects to one matched by name. It stays
 * codec-agnostic: it does not encode audio. SBC encoding and link pacing are done by the stack,
 * which pulls raw PCM from a callback the caller registers via bluetooth_audio_start(); the
 * @ref services_audio_sink Bluetooth backend (sink_bluetooth) supplies that PCM from the
 * pipeline.
 *
 * The controller is configured BR/EDR-only (see sdkconfig.defaults); BLE is off.
 */
#pragma once

#include "esp_err.h"
#include "esp_a2dp_api.h"   /* esp_a2d_source_data_cb_t for the audio data path */
#include <stdbool.h>

/**
 * @defgroup services_bluetooth Bluetooth service
 * @ingroup services
 * @brief Classic Bluetooth A2DP source stack and connection management.
 * @{
 */

/**
 * @brief Bring up the Bluetooth stack in A2DP source mode.
 *
 * Initialises NVS (needed for bonding), the BR/EDR controller and Bluedroid, sets the device
 * name, registers the GAP and A2DP callbacks, starts the A2DP source and advertises one SBC
 * stream endpoint. Pairing is "just works" (no IO capability). Call once at startup.
 * Idempotent.
 *
 * @return ESP_OK; otherwise the first failing controller/Bluedroid/A2DP error.
 */
esp_err_t bluetooth_init(void);

/**
 * @brief Discover nearby devices and connect to the first whose name contains @p name_substr.
 *
 * Starts an inquiry; on each result the device name (from the inquiry record or EIR) is matched
 * against @p name_substr (case-sensitive substring). The first match stops the inquiry and an
 * A2DP source connection is initiated. Connection progress is reported through the log; query
 * bluetooth_is_connected() for the outcome. Requires bluetooth_init().
 *
 * @param name_substr  Substring to look for in advertised device names, e.g. "JBL".
 * @return ESP_OK if the inquiry started; ESP_ERR_INVALID_STATE if not initialised;
 *         ESP_ERR_INVALID_ARG if @p name_substr is NULL/empty; otherwise the GAP error.
 */
esp_err_t bluetooth_connect_by_name(const char *name_substr);

/**
 * @brief Disconnect the current A2DP link, if any.
 *
 * @return ESP_OK (also when nothing is connected), or the underlying A2DP error.
 */
esp_err_t bluetooth_disconnect(void);

/**
 * @brief Report whether an A2DP source connection is currently established.
 *
 * @return true if connected to a sink device, false otherwise.
 */
bool bluetooth_is_connected(void);

/**
 * @brief Start streaming PCM to the connected sink through the A2DP source data path.
 *
 * Registers @p pcm_cb and kicks the media-control handshake (CHECK_SRC_RDY -> START). The
 * stack pulls PCM from @p pcm_cb in its own task and encodes SBC itself; this service stays
 * codec-agnostic and only wires the opaque PCM callback to the link. Requires an established
 * connection.
 *
 * @param pcm_cb  Pull callback supplying interleaved 16-bit PCM at the negotiated rate.
 * @return ESP_OK if the start was issued; ESP_ERR_INVALID_STATE if not connected;
 *         ESP_ERR_INVALID_ARG if @p pcm_cb is NULL; otherwise the A2DP error.
 */
esp_err_t bluetooth_audio_start(esp_a2d_source_data_cb_t pcm_cb);

/**
 * @brief Suspend the media stream started by bluetooth_audio_start().
 *
 * @return ESP_OK if the suspend was issued, otherwise the A2DP error.
 */
esp_err_t bluetooth_audio_stop(void);

/** @} */
