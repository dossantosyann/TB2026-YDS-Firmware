/**
 * @file playlist.h
 * @brief Play order and navigation over the @ref services_storage index: what to play next.
 *
 * A thin state layer above the storage track index (current position + repeat/shuffle mode).
 * It never touches the SD card, the decoder or a sink: it consumes the storage list BY INDEX
 * and answers "which track now / next / previous" as a storage index plus a POSIX path ready
 * for decoder_open(). The transport/player drives it; the dependency direction is
 * player -> playlist -> storage.
 *
 * It does NOT own the SD mount: every read/navigation call fails loud with
 * ESP_ERR_INVALID_STATE when storage is not ready, and ESP_ERR_NOT_FOUND on an empty list,
 * rather than ever exposing a silently invalid current index.
 *
 * State is static and minimal (one position, two mode flags, one permutation table); no
 * allocation happens per call. Call @ref playlist_sync once after storage_init/storage_rescan
 * to (re)bind to the current index before navigating.
 */
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @defgroup services_audio_playlist Playlist service
 * @ingroup services
 * @brief Current track + next/prev navigation over the storage index (no hardware).
 * @{
 */

/** @brief How the ends of the list and end-of-track behave. */
typedef enum {
    PLAYLIST_REPEAT_OFF = 0,  /**< No wrap: next past the last (or prev before the first) is end-of-list. */
    PLAYLIST_REPEAT_ALL,      /**< Wrap: last->first and first->last on next/prev. */
    PLAYLIST_REPEAT_ONE,      /**< Same wrap as ALL for navigation; the transport reads this to replay the current track on natural end. */
} playlist_repeat_t;

/** @brief The resolved current track, filled by the navigation/read calls. */
typedef struct {
    size_t      index;  /**< 0-based storage index of the track. */
    const char *path;   /**< storage_track_path(index): full POSIX path for decoder_open(). */
    const char *name;   /**< storage_track_name(index): display name. */
} playlist_track_t;

/**
 * @brief (Re)bind the playlist to the current storage index.
 *
 * Call once after storage_init() and again after storage_rescan(). Rebuilds the play order
 * for the current track count (identity, or a fresh shuffle when shuffle is on) and clamps the
 * current position into range (if it fell past the new end it moves to the last track; an empty
 * list leaves no current track). Requires storage to be ready.
 *
 * @return ESP_OK; ESP_ERR_INVALID_STATE if storage is not ready.
 */
esp_err_t playlist_sync(void);

/**
 * @brief Read the current track without changing the position.
 * @param[out] out  Filled with the current track; may be NULL to query state only.
 * @return ESP_OK; ESP_ERR_INVALID_STATE if storage is not ready; ESP_ERR_NOT_FOUND if empty.
 */
esp_err_t playlist_current(playlist_track_t *out);

/**
 * @brief Advance to the next track and return it.
 *
 * Wraps to the first track when repeat is ALL/ONE; with repeat OFF, advancing past the last
 * track leaves the position unchanged and returns ESP_ERR_NOT_FOUND (end of list).
 *
 * @param[out] out  Filled with the new current track; may be NULL.
 * @return ESP_OK; ESP_ERR_INVALID_STATE if storage is not ready; ESP_ERR_NOT_FOUND if empty
 *         or at the end with repeat OFF.
 */
esp_err_t playlist_next(playlist_track_t *out);

/**
 * @brief Step to the previous track and return it. Mirror of @ref playlist_next.
 * @param[out] out  Filled with the new current track; may be NULL.
 * @return ESP_OK; ESP_ERR_INVALID_STATE if storage is not ready; ESP_ERR_NOT_FOUND if empty
 *         or at the start with repeat OFF.
 */
esp_err_t playlist_prev(playlist_track_t *out);

/**
 * @brief Jump to a specific storage track and make it current.
 * @param index     0-based storage index (as listed by the storage service / UI browser).
 * @param[out] out  Filled with the selected track; may be NULL.
 * @return ESP_OK; ESP_ERR_INVALID_STATE if storage is not ready; ESP_ERR_INVALID_ARG if
 *         @p index is out of range.
 */
esp_err_t playlist_select(size_t index, playlist_track_t *out);

/**
 * @brief Set the repeat mode. Default @ref PLAYLIST_REPEAT_ALL.
 * @param mode  The repeat mode.
 */
void playlist_set_repeat(playlist_repeat_t mode);

/** @brief Current repeat mode (for the UI and the transport's end-of-track policy). */
playlist_repeat_t playlist_get_repeat(void);

/**
 * @brief Turn shuffle on or off. Default off.
 *
 * Rebuilds the play order while keeping the current track playing (it becomes the new order's
 * starting point). Off restores ascending storage order. Takes effect immediately when storage
 * is ready, otherwise on the next @ref playlist_sync.
 *
 * @param on  true to shuffle, false for ascending order.
 */
void playlist_set_shuffle(bool on);

/** @brief Whether shuffle is on. */
bool playlist_get_shuffle(void);

/* ---- queue view/edit (for the playlist browser UI) --------------------- */
/* The "queue" is the play order from the current track onward: row 0 is the
   track playing now, rows >= 1 are the upcoming tracks in play order. Edits
   only touch the upcoming region, so the current track keeps playing. These
   are session edits over the play order; a playlist_sync() (rescan/reboot)
   rebuilds the full order and any removed track reappears. */

/** @brief Number of tracks from the current one onward (current + upcoming); 0 if empty/not ready. */
size_t playlist_queue_len(void);

/**
 * @brief Read the queue entry at @p row (0 = current track) without changing the position.
 * @param row       0-based offset from the current track.
 * @param[out] out  Filled with the track; must not be NULL.
 * @return ESP_OK; ESP_ERR_INVALID_STATE if storage is not ready; ESP_ERR_NOT_FOUND if @p row
 *         is past the end.
 */
esp_err_t playlist_queue_at(size_t row, playlist_track_t *out);

/**
 * @brief Move the upcoming track at @p row one step earlier in the queue.
 *
 * Only upcoming tracks move, and never above the current track: @p row must be >= 2.
 *
 * @param row  Queue row to move (>= 2).
 * @return ESP_OK; ESP_ERR_INVALID_STATE if storage is not ready; ESP_ERR_INVALID_ARG if @p row
 *         is the current track, the first upcoming track, or out of range.
 */
esp_err_t playlist_queue_move_up(size_t row);

/**
 * @brief Move the upcoming track at @p row one step later in the queue.
 * @param row  Queue row to move (>= 1, and not the last row).
 * @return ESP_OK; ESP_ERR_INVALID_STATE if storage is not ready; ESP_ERR_INVALID_ARG if @p row
 *         is the current track, the last row, or out of range.
 */
esp_err_t playlist_queue_move_down(size_t row);

/**
 * @brief Remove the upcoming track at @p row from the play order for this session.
 *
 * Non-destructive: only the play order shrinks (the file stays on the card and reappears on the
 * next playlist_sync()). The current track (@p row 0) cannot be removed.
 *
 * @param row  Queue row to remove (>= 1).
 * @return ESP_OK; ESP_ERR_INVALID_STATE if storage is not ready; ESP_ERR_INVALID_ARG if @p row
 *         is the current track or out of range.
 */
esp_err_t playlist_queue_remove(size_t row);

/** @} */
