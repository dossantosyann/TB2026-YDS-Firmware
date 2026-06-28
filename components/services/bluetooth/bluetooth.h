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
#include "esp_a2dp_api.h"     /* esp_a2d_source_data_cb_t for the audio data path */
#include "esp_gap_bt_api.h"   /* esp_bd_addr_t, ESP_BT_GAP_MAX_BDNAME_LEN */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

/** @brief Maximum number of devices retained from a single scan. */
#define BLUETOOTH_MAX_DEVICES 16

/** @brief One device found during a scan; the unit the UI lists and selects from. */
typedef struct {
    esp_bd_addr_t bda;                              /**< Bluetooth device address (the identity). */
    char          name[ESP_BT_GAP_MAX_BDNAME_LEN + 1]; /**< Advertised name; "" until it resolves. */
    int8_t        rssi;                             /**< Signal strength in dBm; INT8_MIN if unknown. */
} bluetooth_device_t;

/**
 * @brief Start an inquiry that collects nearby devices into a list (does not auto-connect).
 *
 * Clears the device list and starts a general inquiry. Results accumulate in the background
 * (deduplicated by address; a name that arrives after the address updates the existing entry);
 * read them with bluetooth_get_devices(). The inquiry stops on its own after the inquiry window
 * or when bluetooth_scan_stop()/bluetooth_connect() is called. Query bluetooth_is_scanning().
 *
 * Caller responsibility: an inquiry floods the radio and breaks an active A2DP stream, so the
 * caller must pause Bluetooth playback before scanning (the service stays player-agnostic and
 * does not do this itself). Requires bluetooth_init().
 *
 * @return ESP_OK if the inquiry started; ESP_ERR_INVALID_STATE if not initialised; otherwise the
 *         GAP error.
 */
esp_err_t bluetooth_scan_start(void);

/**
 * @brief Stop an inquiry started by bluetooth_scan_start(). No-op if not scanning.
 * @return ESP_OK, or the underlying GAP error.
 */
esp_err_t bluetooth_scan_stop(void);

/**
 * @brief Report whether an inquiry is currently running.
 * @return true while scanning (cleared when the inquiry ends, is stopped, or a connect begins).
 */
bool bluetooth_is_scanning(void);

/**
 * @brief Copy the current scan results into @p out (thread-safe snapshot).
 *
 * Copies up to @p cap entries from the internal list under a lock, so it is safe to call from
 * the UI task while the inquiry fills the list from the Bluetooth task.
 *
 * @param out  Destination array; must hold at least @p cap entries. Ignored if NULL.
 * @param cap  Capacity of @p out.
 * @return Number of devices copied (min of the list size and @p cap); 0 if @p out is NULL.
 */
size_t bluetooth_get_devices(bluetooth_device_t *out, size_t cap);

/**
 * @brief Connect the A2DP source to a device chosen by address (e.g. a scan result).
 *
 * Cancels any running inquiry, then initiates the connection. Connection progress is reported
 * through the log; query bluetooth_is_connected() for the outcome. Requires bluetooth_init().
 *
 * @param bda  Address to connect to, typically taken from a bluetooth_device_t.
 * @return ESP_OK if the connect was issued; ESP_ERR_INVALID_STATE if not initialised;
 *         ESP_ERR_INVALID_ARG if @p bda is NULL; otherwise the A2DP error.
 */
esp_err_t bluetooth_connect(const esp_bd_addr_t bda);

/**
 * @brief TEST ONLY — scan and connect to the first device whose name contains @p name_substr.
 *
 * Convenience for bring-up/host tests: starts an inquiry and auto-connects to the first name
 * match. Not used by the final build (the UI uses bluetooth_scan_start() + bluetooth_connect()).
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

/** @brief Coarse connection state for the UI to render (connecting / connected / failure). */
typedef enum {
    BLUETOOTH_CONN_IDLE,        /**< No link and no attempt in progress. */
    BLUETOOTH_CONN_CONNECTING,  /**< A connect is in progress (set when issued, cleared on result). */
    BLUETOOTH_CONN_CONNECTED,   /**< A2DP link established. */
    BLUETOOTH_CONN_FAILED,      /**< The last attempt failed, or an established link dropped abnormally. */
} bluetooth_conn_state_t;

/**
 * @brief Read the current connection state.
 *
 * Richer than bluetooth_is_connected(): lets the UI show a "connecting…" spinner while an attempt
 * is in flight and a failure message when it does not complete. The state stays
 * @ref BLUETOOTH_CONN_FAILED until the next connect attempt or a clean disconnect.
 *
 * @return The current @ref bluetooth_conn_state_t.
 */
bluetooth_conn_state_t bluetooth_get_conn_state(void);

/**
 * @brief The identity of a remembered device, enough to reconnect without scanning again.
 *
 * Address + name; the address is what reconnection actually needs (the link keys are already
 * bonded in NVS by Bluedroid), the name is only for display.
 */
typedef struct {
    esp_bd_addr_t bda;                                 /**< Address of the remembered device. */
    char          name[ESP_BT_GAP_MAX_BDNAME_LEN + 1]; /**< Its name for display; "" if unknown. */
} bt_known_device_t;

/** @brief Maximum number of paired devices remembered and persisted across reboots (LRU). */
#define BLUETOOTH_MAX_KNOWN 8

/**
 * @brief Copy the persisted list of known (paired) devices into @p out, most-recent-first.
 *
 * The list is restored from NVS at bluetooth_init() and updated on each successful connection;
 * s_known[0] is the same device bluetooth_get_last_device() returns. Use it to show the paired
 * devices for quick reconnect after a reboot, without scanning.
 *
 * @param out  Destination array; must hold at least @p cap entries. Ignored if NULL.
 * @param cap  Capacity of @p out.
 * @return Number of devices copied (min of the list size and @p cap); 0 if @p out is NULL.
 */
size_t bluetooth_get_known_devices(bt_known_device_t *out, size_t cap);

/**
 * @brief Read the last successfully connected device (the one to offer for quick reconnect).
 *
 * The most-recent entry of the known-device list (see bluetooth_get_known_devices()). Set
 * automatically on each successful connection and persisted to NVS by this service, so it
 * survives a reboot.
 *
 * @param[out] out  Filled with the remembered device. Ignored if NULL.
 * @return true if a device is remembered, false if none yet (then @p out is untouched).
 */
bool bluetooth_get_last_device(bt_known_device_t *out);

/**
 * @brief Remember a device as the most-recent one and persist it to NVS.
 *
 * Inserts @p dev at the front of the known-device list (or moves it there) and saves the list, so
 * bluetooth_reconnect_last() can target it. Normally the service does this itself on each
 * connection; this is the manual entry point. Does not connect by itself. The bond (link keys)
 * must still exist in NVS for a silent reconnect.
 *
 * @param dev  Device to remember. Ignored if NULL.
 */
void bluetooth_set_last_device(const bt_known_device_t *dev);

/**
 * @brief Reconnect to the remembered device by address, without scanning.
 *
 * Manual reconnect (no auto-reconnect at boot by design): the UI calls this from the menu. Works
 * on a bonded device because pairing is already stored. Watch bluetooth_get_conn_state() for the
 * outcome.
 *
 * @return ESP_OK if the connect was issued; ESP_ERR_INVALID_STATE if not initialised;
 *         ESP_ERR_NOT_FOUND if no device is remembered; otherwise the A2DP error.
 */
esp_err_t bluetooth_reconnect_last(void);

/**
 * @brief Forget a known device: drop it from the persisted list and remove its bond from NVS.
 *
 * Removes the matching entry from the known-device list (saving the updated list to NVS) and
 * removes its bond (link keys) via esp_bt_gap_remove_bond_device(). After this, the device no
 * longer reconnects and must be paired again from a fresh scan. The bond is removed even if the
 * address was not in the list.
 *
 * @param bda  Address of the device to forget (e.g. from bluetooth_get_known_devices()).
 * @return ESP_OK; ESP_ERR_INVALID_ARG if @p bda is NULL; otherwise the bond-removal error.
 */
esp_err_t bluetooth_forget_device(const esp_bd_addr_t bda);

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

/**
 * @brief Set the connected speaker's absolute volume over AVRCP (controller role).
 *
 * Sends an AVRCP SetAbsoluteVolume to the sink. The AVRCP control channel comes up a few
 * seconds after the A2DP connection, so a call made before then is remembered and applied
 * automatically once AVRCP connects. The speaker applies and renders the level itself; this
 * does not attenuate the PCM stream.
 *
 * @param volume  Target volume, 0 (silent) to 0x7F (loudest); values above 0x7F are clamped.
 * @return ESP_OK if the command was issued or deferred; otherwise the AVRCP error.
 */
esp_err_t bluetooth_set_absolute_volume(uint8_t volume);

/**
 * @brief Report whether the AVRCP control channel is established.
 *
 * AVRCP connects a few seconds after the A2DP link. Useful before streaming if you want the
 * volume controllable from the first sample (otherwise the speaker starts at its own default).
 *
 * @return true if AVRCP is connected, false otherwise.
 */
bool bluetooth_is_avrc_connected(void);

/**
 * @brief Report whether the speaker has confirmed the most recent absolute-volume set.
 *
 * Cleared when bluetooth_set_absolute_volume() sends a command, set when the sink's
 * SetAbsoluteVolume response arrives. Poll this before streaming the first sample so the
 * speaker is known to be at the intended (e.g. silent) level — never blast the user's ears.
 *
 * @return true once the sink has acknowledged the last volume set.
 */
bool bluetooth_volume_acked(void);

/** @} */
