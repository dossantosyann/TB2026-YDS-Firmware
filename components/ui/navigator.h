/**
 * @file navigator.h
 * @brief Screen-stack router: push/pop screens and dispatch input to the top one.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "screen.h"
#include <stdint.h>

/**
 * @defgroup ui_navigator Navigator
 * @ingroup ui
 * @brief A fixed-depth stack of screens; the top screen owns input and rendering.
 * @{
 */

/**
 * @brief Push a screen onto the stack and call its on_enter.
 * @param screen  Screen to activate. Ignored if the stack is full.
 */
void navigator_push(screen_t *screen);

/**
 * @brief Pop the top screen and call its on_exit. No-op if the stack is empty.
 */
void navigator_pop(void);

/**
 * @brief Deliver one input event to the top screen, then render it.
 * @param event  Input event to dispatch. No-op if the stack is empty.
 */
void navigator_tick(ui_event_t event);

/**
 * @brief Re-render the top screen and the status bar, without an input event.
 *
 * Used by the UI loop to refresh a live screen on its timer tick. No-op if the
 * stack is empty.
 */
void navigator_render(void);

/**
 * @brief How long the UI task should wait for input before a live re-render.
 *
 * Returns the top screen's @ref screen_t::refresh_ms as ticks, or portMAX_DELAY when
 * it is 0 (event-driven) or the stack is empty — so a non-live screen blocks forever
 * and costs no periodic wake-ups.
 *
 * @return FreeRTOS tick count to pass to input_get_event() (portMAX_DELAY when no
 *         live refresh is wanted).
 */
uint32_t navigator_refresh_ticks(void);

/** @} */
