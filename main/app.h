/**
 * @file app.h
 * @brief Application entry points: board bring-up and UI hand-off.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

/**
 * @brief Bring up the whole board: self-hold power, NVS, drivers and services.
 *
 * Runs the full boot sequence (EnableReg self-hold, PM config, NVS, then the
 * bsp/driver/service init chain) so the system is ready before the UI starts.
 * Called once from app_main().
 */
void app_init(void);

/**
 * @brief Start the UI on its own task and return.
 *
 * Spawns the pinned UI task so app_main() can exit (its undersized stack would
 * otherwise overflow running the UI loop). Call once, after app_init().
 */
void app_run(void);
