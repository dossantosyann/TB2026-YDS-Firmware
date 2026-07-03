/**
 * @file player.h
 * @brief Playback transport: the orchestrator that turns the playlist into sound.
 *
 * The player sits above the pieces and wires them together: it asks the @ref
 * services_audio_playlist what to play, hands the path to the @ref services_audio_pipeline,
 * picks the output sink and keeps the @ref services_audio_volume routing in step, and listens
 * for end-of-track to advance automatically. It owns only the transport state (stopped /
 * playing / paused) and the chosen output; "which track" stays the playlist's single source of
 * truth, so the player carries no duplicate position. Dependency direction: ui -> player ->
 * playlist -> storage; the player never touches the SD mount or a decoder/sink directly.
 *
 * Threading: call the player_* API from one control task (the UI/maintenance task). The only
 * exception is the internal end-of-track callback, which runs in the audio task and merely
 * advances; do not call the API from interrupt context.
 */
#pragma once

#include "esp_err.h"
#include "playlist.h"
#include "volume.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup services_audio_player Playback transport
 * @ingroup services
 * @brief play/pause/resume/stop/next/prev over the playlist, with auto-advance on EOF.
 * @{
 */

/** @brief Transport state. */
typedef enum {
    PLAYER_STOPPED,  /**< Nothing playing. */
    PLAYER_PLAYING,  /**< A track is streaming to the active output. */
    PLAYER_PAUSED,   /**< Playback suspended; the sink is down, the stream position kept. */
} player_state_t;

/** @brief Snapshot of the transport for the UI. */
typedef struct {
    player_state_t   state;       /**< Current transport state. */
    playlist_track_t track;       /**< Current track; valid only when @p state is not STOPPED. */
    uint32_t         elapsed_ms;  /**< Playback position in ms (0 when stopped). */
    uint32_t         total_ms;    /**< Track duration in ms; 0 if unknown or stopped. */
    bool             track_unsupported; /**< The last playback stopped because the active output
                                             refused the track's format (e.g. non-44.1 kHz over
                                             Bluetooth). Cleared by the next play/stop action. */
} player_status_t;

/**
 * @brief Register the player with the pipeline's end-of-track callback. Call once at startup.
 *
 * @return ESP_OK.
 */
esp_err_t player_init(void);

/**
 * @brief Select the output and keep the volume routing consistent.
 *
 * Points the pipeline at the matching sink and tells the volume service where to send the pot.
 * Selecting the output already in use is a no-op (playback is not disturbed). If a track is
 * currently playing the live stream is re-routed onto the new sink immediately, continuing from
 * its current position (the decoder stays open, no restart); otherwise the choice is just
 * remembered for the next playback. This is the single switch point for both manual selection
 * and a future automatic BT connect/disconnect switch.
 *
 * Bluetooth is referenced only on this path, so a build without BT still links. Switching to
 * VOLUME_OUT_BT primes the speaker's volume and, if a stream is live, holds it until the speaker
 * acknowledges that volume before streaming (never blast) -- the switch itself succeeds and
 * player_poll() completes the deferred start. It is refused only when no speaker is connected.
 * VOLUME_OUT_NONE is a valid software mute that cuts the active output.
 *
 * @param out  VOLUME_OUT_DAC (jack), VOLUME_OUT_BT (speaker), or VOLUME_OUT_NONE (mute).
 * @return ESP_OK; ESP_ERR_INVALID_STATE if @p out is Bluetooth and no speaker is connected;
 *         ESP_ERR_INVALID_ARG for any other value; otherwise the pipeline error if a live
 *         re-route fails.
 */
esp_err_t player_set_output(volume_output_t out);

/**
 * @brief Complete a deferred Bluetooth start once the speaker acknowledges the volume.
 *
 * Call periodically from the maintenance loop (after volume_poll()). A switch/play to a
 * Bluetooth speaker holds the stream until the volume is acknowledged (never blast); this drives
 * that held start to completion. A no-op when nothing is pending.
 */
void player_poll(void);

/**
 * @brief Play the storage track at @p index (stops any current stream first).
 *
 * @param index  0-based storage index (as listed by the UI browser).
 * @return ESP_OK; ESP_ERR_INVALID_STATE if storage is not ready, or if the output is
 *         Bluetooth and the speaker has not yet acknowledged the volume (never blast);
 *         ESP_ERR_INVALID_ARG if @p index is out of range; otherwise the pipeline error.
 */
esp_err_t player_play(size_t index);

/**
 * @brief Start playback from the stopped state (e.g. first play after boot).
 *
 * Plays the playlist's current track. When shuffle is on, the play order is re-shuffled first so
 * a random track starts (see @ref playlist_random). A no-op unless currently stopped.
 *
 * @return ESP_OK; ESP_ERR_INVALID_STATE if not stopped, storage is not ready, or the output is
 *         Bluetooth and the speaker has not yet acknowledged the volume (never blast); otherwise
 *         the pipeline/playlist error.
 */
esp_err_t player_start(void);

/**
 * @brief Pause the current playback (sink down, position kept).
 * @return ESP_OK; ESP_ERR_INVALID_STATE if not currently playing.
 */
esp_err_t player_pause(void);

/**
 * @brief Resume a paused playback.
 * @return ESP_OK; ESP_ERR_INVALID_STATE if not currently paused.
 */
esp_err_t player_resume(void);

/**
 * @brief Stop playback and power the output down.
 * @return ESP_OK.
 */
esp_err_t player_stop(void);

/**
 * @brief Skip to the next track (respecting the playlist's repeat/shuffle order).
 * @return ESP_OK; ESP_ERR_INVALID_STATE if stopped; ESP_ERR_NOT_FOUND at the end of the list
 *         with repeat off (playback stops); otherwise the pipeline/playlist error.
 */
esp_err_t player_next(void);

/**
 * @brief Step to the previous track. Mirror of @ref player_next.
 * @return As @ref player_next.
 */
esp_err_t player_prev(void);

/**
 * @brief Read the current transport state and track.
 * @param[out] out  Filled with the snapshot. Must not be NULL.
 * @return ESP_OK; ESP_ERR_INVALID_ARG if @p out is NULL.
 */
esp_err_t player_get_state(player_status_t *out);

/** @} */
