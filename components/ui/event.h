/**
 * @file event.h
 * @brief Input event vocabulary shared by screens and the navigator.
 */
#pragma once

/**
 * @defgroup ui_event Input events
 * @ingroup ui
 * @brief The input events a screen can receive.
 * @{
 */

/** @brief A single UI input event, produced from button/encoder input. */
typedef enum {
    UI_EVENT_UP,      /**< Move selection up / to the previous item. */
    UI_EVENT_DOWN,    /**< Move selection down / to the next item. */
    UI_EVENT_SELECT,  /**< Activate the current item. */
    UI_EVENT_BACK,    /**< Go back / pop the current screen. */
    UI_EVENT_LEFT,    /**< Move/seek left (previous). */
    UI_EVENT_RIGHT,   /**< Move/seek right (next). */
} ui_event_t;

/** @} */
