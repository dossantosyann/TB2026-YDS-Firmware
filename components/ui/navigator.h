/**
 * @file navigator.h
 * @brief Screen-stack router: push/pop screens and dispatch input to the top one.
 */
#pragma once

#include "screen.h"

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

/** @} */
