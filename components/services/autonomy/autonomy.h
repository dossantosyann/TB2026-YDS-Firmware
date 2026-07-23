/**
 * @file autonomy.h
 * @brief Battery autonomy self-test: run a workload on battery until shutdown, log a curve.
 *
 * Drives a characterisation run: the user charges to 100%, picks a workload, and the board runs
 * on battery until it shuts itself off, logging a discharge curve so the autonomy can be read
 * back from an SD CSV on the next boot. The service owns the run state and the logging;
 * the in-progress UI is a screensaver-like mode in the UI task, and the maintenance task drives
 * autonomy_tick().
 *
 * Storage: the discharge curve accumulates in RAM and is rewritten to a single NVS blob on every
 * sample (bounded to a fixed sample count by halving the rate when full), so the run survives any
 * shutdown — graceful or a crash. On the next boot autonomy_boot_export() writes it out as a CSV
 * on the SD card. The NVS partition here is only 24 KB, hence the bound rather than a full-detail
 * curve.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup services_autonomy Autonomy test service
 * @ingroup services
 * @brief Self-measured battery autonomy: workload run + discharge-curve logging.
 * @{
 */

/** @brief Which workload the run exercises. */
typedef enum {
    AUTONOMY_TEST_IDLE = 0,  ///< No playback, standard idle.
    AUTONOMY_TEST_JACK,      ///< Looped playback to the wired jack (DAC).
    AUTONOMY_TEST_BT,        ///< Looped playback to a connected Bluetooth sink.
} autonomy_test_t;

/** @brief Outcome of the most recent run, for the UI's "Last run" block. */
typedef enum {
    AUTONOMY_RESULT_NONE = 0,   ///< No run recorded yet.
    AUTONOMY_RESULT_CANCELLED,  ///< The user cancelled the run (B).
    AUTONOMY_RESULT_COMPLETED,  ///< The run ran to battery shutdown.
    AUTONOMY_RESULT_ABORTED,    ///< USB was plugged mid-run: the measurement is invalid.
    AUTONOMY_RESULT_INTERRUPTED,///< The run never finished cleanly (crash/reset mid-test); partial data.
} autonomy_result_t;

/** @brief Snapshot of the running test, for the in-progress screen. */
typedef struct {
    bool            running;     ///< true between autonomy_start() and cancel/shutdown.
    autonomy_test_t type;        ///< Workload of the running test (valid when @p running).
    uint32_t        elapsed_ms;  ///< Time since the run started, ms (0 when not running).
} autonomy_status_t;

/**
 * @brief Start an autonomy run with the given workload.
 *
 * IDLE stops playback (silence). JACK and BT measure whatever is already playing on the respective
 * output: the caller must have a track playing on the DAC (JACK) or on a connected Bluetooth sink
 * (BT) first. The run pins that stream down for the duration — it forces loop-one (so it never ends
 * early) and a fixed pot-independent volume (JACK 50%, BT a near-silent 1% — BT amplifies in the
 * externally powered speaker, so its level does not affect this board's draw) — and restores both
 * when the run ends.
 *
 * @param type  Workload to exercise.
 * @return ESP_OK; ESP_ERR_INVALID_STATE if a run is already in progress, or if nothing is playing
 *         (JACK/BT), or if no Bluetooth sink is connected (BT).
 */
esp_err_t autonomy_start(autonomy_test_t type);

/**
 * @brief Cancel the running test, keeping it as the last (cancelled) result. No-op if idle.
 */
void autonomy_cancel(void);

/** @brief Whether a run is currently in progress. */
bool autonomy_is_running(void);

/**
 * @brief Copy the running test's snapshot.
 * @param[out] out  Receives the snapshot; ignored if NULL.
 */
void autonomy_get_status(autonomy_status_t *out);

/**
 * @brief Periodic hook, called from the maintenance task (after power_tick()).
 *
 * Samples the discharge curve at the current interval and persists it. While a run is in
 * progress it also owns the end: once the cell is near-empty (power_soc_at_shutdown(), on
 * battery, not charging) it writes the final log and calls power_shutdown(). No-op when no
 * run is in progress.
 */
void autonomy_tick(void);

/**
 * @brief Export a stored (not-yet-exported) run to a CSV on the SD card. Call once at boot.
 *
 * Reads the NVS run blob; if one is present and not yet exported, writes it as a CSV to the SD
 * card, marks it exported, and populates the "last run" fields for the UI. Requires the SD card
 * to be mounted. A no-op if there is no stored run. Also loads the last-run summary for the UI
 * even when the run was already exported on an earlier boot.
 *
 * @return ESP_OK if a run was exported (or there was nothing to do); an esp_err_t on SD/NVS error.
 */
esp_err_t autonomy_boot_export(void);

/**
 * @brief Re-export the last stored run to a fresh CSV on the SD card (without rebooting).
 *
 * Writes the run currently held in RAM (loaded at boot by autonomy_boot_export(), or the one that
 * just ended this session) out to a new uniquely-named CSV, to confirm the SD write path. A no-op
 * while a run is in progress.
 *
 * @return ESP_OK on write; ESP_ERR_INVALID_STATE if a run is in progress; ESP_ERR_NOT_FOUND if no
 *         run is stored; otherwise the SD/NVS error from the export.
 */
esp_err_t autonomy_redump(void);

/** @brief Outcome of the most recent run (AUTONOMY_RESULT_NONE until one happens). */
autonomy_result_t autonomy_get_last_result(void);

/** @brief Workload of the most recent run (valid when the last result is not NONE). */
autonomy_test_t autonomy_get_last_type(void);

/** @brief Duration of the most recent run in seconds (0 if none). */
uint32_t autonomy_get_last_duration_s(void);

/** @brief Whether the most recent run has been successfully written to the SD card. */
bool autonomy_get_last_dump_ok(void);

/** @} */
