/**
 * @file storage.h
 * @brief Track enumeration over the microSD card: the index of playable files.
 *
 * A thin layer above POSIX readdir and the @ref drivers_sdcard driver. It owns the SD
 * mount lifecycle (storage_init mounts, storage_deinit unmounts) so the playlist/player
 * never touches the driver directly, then scans the card root once and serves the result
 * from an in-memory list: no re-scan on access (power), nothing happens with no card.
 *
 * The list holds only the audio files the decoder can open (.mp3 / .wav, case-insensitive),
 * sorted alphabetically (case-insensitive) so next/prev is deterministic. Each entry is a
 * full POSIX path ready to hand to decoder_open(); the display name is the part after the
 * last '/'. No metadata, no recursion, no persistent cache: just the flat index.
 */
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @defgroup services_storage Storage service
 * @ingroup services
 * @brief Enumerate the SD card's playable tracks and serve them by index.
 * @{
 */

/** @brief Largest number of tracks held in the in-memory index. Overflow fails loud. */
#define STORAGE_MAX_TRACKS  512

/** @brief Size of one stored path buffer: "/sdcard/" + FATFS LFN (255) + nul, rounded up. */
#define STORAGE_PATH_MAX    320

/**
 * @brief Mount the SD card and scan its root for playable tracks.
 *
 * Checks card presence, mounts the FAT volume via @ref sdcard_mount, then scans
 * @ref SDCARD_MOUNT_POINT. Idempotent: a second call while already ready is a no-op.
 *
 * @return ESP_OK; ESP_ERR_NOT_FOUND if no card is inserted; the @ref sdcard_mount error
 *         if mounting fails; ESP_FAIL if the root cannot be read; ESP_ERR_NO_MEM if the
 *         card holds more than @ref STORAGE_MAX_TRACKS tracks (never truncated silently).
 */
esp_err_t storage_init(void);

/**
 * @brief Re-scan the (already mounted) card root, rebuilding the track index.
 *
 * Use after a card is (re)inserted and remounted. Requires the volume to be mounted.
 *
 * @return ESP_OK; ESP_ERR_INVALID_STATE if not mounted; otherwise as @ref storage_init.
 */
esp_err_t storage_rescan(void);

/**
 * @brief Whether the card is mounted and a scan has completed.
 * @return true if the index is usable (it may still hold zero tracks on an empty card).
 */
bool storage_ready(void);

/** @brief Number of tracks in the index (0 before any successful scan). */
size_t storage_count(void);

/**
 * @brief Full POSIX path of a track, ready for decoder_open().
 * @param index  0-based track index.
 * @return The path, or NULL if @p index is out of range.
 */
const char *storage_track_path(size_t index);

/**
 * @brief Display name of a track (file name, no directory prefix).
 * @param index  0-based track index.
 * @return The name, or NULL if @p index is out of range.
 */
const char *storage_track_name(size_t index);

/**
 * @brief Scan an arbitrary directory for playable tracks, rebuilding the index.
 *
 * The pure enumeration core: opendir/readdir + extension filter + sort, parameterised by
 * directory. @ref storage_init and @ref storage_rescan wrap it onto @ref SDCARD_MOUNT_POINT;
 * the host tests call it on a temporary folder, so the logic is exercised without hardware.
 *
 * @param root  Directory to scan (no trailing slash), e.g. "/sdcard".
 * @return ESP_OK (possibly 0 tracks); ESP_FAIL if @p root cannot be opened;
 *         ESP_ERR_NO_MEM on allocation failure or more than @ref STORAGE_MAX_TRACKS tracks.
 */
esp_err_t storage_scan_dir(const char *root);

/**
 * @brief Unmount the card and free the index.
 * @return ESP_OK, or the @ref sdcard_unmount error.
 */
esp_err_t storage_deinit(void);

/** @} */
