/**
 * @file diag.h
 * @brief Runtime diagnostics: FreeRTOS stack occupancy, run-time stats, heap usage.
 *
 * Measurement instrument for the characterisation campaign, not a product feature.
 * The whole module hangs off DIAG_ENABLED below: left at 0, every entry point is an
 * empty inline stub, diag.c compiles to an empty translation unit, and the linker
 * emits nothing — zero bytes of flash, RAM and CPU in the final build.
 *
 * The API is deliberately free of FreeRTOS types so this header can be included
 * anywhere (including from the UI layer, where pulling in freertos/FreeRTOS.h clashes
 * with picolibc's on_exit). The handle of a registering task is read from inside the
 * module with xTaskGetCurrentTaskHandle().
 *
 * Enabling this requires the FreeRTOS trace/stats options in sdkconfig.defaults
 * (see the "Diagnostics" block there); diag.c fails the build loudly if they are off.
 * Those options carry their own permanent cost (a bigger TCB, a run-time counter read
 * on every context switch), so they belong in the final build no more than this module
 * does — turn them off together.
 */
#pragma once

#include <stdint.h>

/**
 * @defgroup services_diag Diagnostics service
 * @ingroup services
 * @brief Opt-in FreeRTOS stack / run-time / heap measurement for the characterisation campaign.
 * @{
 */

/** Master switch. 0 = the module does not exist in the binary. */
#define DIAG_ENABLED 0

/** Stack occupancy above this percentage raises the alert column in the dump. */
#define DIAG_ALERT_PCT 75

/** Period at which the maintenance task dumps the report, in milliseconds. */
#define DIAG_DUMP_MS 10000

#if DIAG_ENABLED

/**
 * @brief Register the calling task in the diagnostics registry.
 *
 * Called by each permanent task (ui, audio, input, maintenance) as its first statement,
 * so the registry learns the handle without any creation site having to keep one.
 *
 * @param stack_bytes  Stack size the task was created with (the xTaskCreate argument,
 *                     which is in bytes on ESP-IDF).
 */
void diag_register_task(uint32_t stack_bytes);

/**
 * @brief Log the calling task's stack high water mark, for a task about to delete itself.
 *
 * The ephemeral tasks (storage_scan, sd_scan, bt_reset, bt_off, usbroute) are gone by the
 * time any periodic dump runs, so this is the only point where their stack use can still be
 * measured. Call it immediately before vTaskDelete(NULL).
 */
void diag_task_exit(void);

/**
 * @brief Accumulate elapsed time and dump the report once DIAG_DUMP_MS has passed.
 *
 * Called from the maintenance loop on every iteration; keeping the accounting here means
 * the loop needs no diagnostics-only state of its own.
 *
 * @param elapsed_ms  Time since the previous call (the maintenance loop period).
 */
void diag_tick(uint32_t elapsed_ms);

/**
 * @brief Log the full diagnostics report: registered tasks, vTaskList(),
 *        vTaskGetRunTimeStats(), and internal/PSRAM heap usage.
 */
void diag_dump_tasks(void);

#else /* !DIAG_ENABLED — the module compiles away */

static inline void diag_register_task(uint32_t stack_bytes) { (void)stack_bytes; }
static inline void diag_task_exit(void) {}
static inline void diag_tick(uint32_t elapsed_ms) { (void)elapsed_ms; }
static inline void diag_dump_tasks(void) {}

#endif /* DIAG_ENABLED */

/** @} */
