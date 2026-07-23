/**
 * @file screen.h
 * @brief Screen interface contract: the vtable every screen implements.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 *
 * @defgroup ui User Interface
 * @brief Framebuffer rendering, the screen interface, navigation and input events.
 */
#pragma once

#include "event.h"

/**
 * @defgroup ui_screen Screen interface
 * @ingroup ui
 * @brief The polymorphic screen contract, as a manual C "vtable".
 * @{
 */

/** @brief Opaque forward declaration; each concrete screen embeds this first. */
typedef struct screen_t screen_t;

/**
 * @brief A screen's behaviour, as a table of function pointers.
 *
 * Concrete screens put this struct first, so their pointer can be cast to screen_t*.
 */
struct screen_t {
    void (*on_enter)(screen_t *self);                       /**< Called when the screen is pushed. */
    void (*on_exit)(screen_t *self);                        /**< Called when the screen is popped. */
    void (*handle_input)(screen_t *self, ui_event_t event); /**< Process one input event. */
    void (*render)(screen_t *self);                         /**< Draw the screen into the framebuffer. */
    unsigned refresh_ms;  /**< 0 (default): event-driven, the UI task blocks until input. >0: the
                               task wakes every refresh_ms to re-render this screen, for live data
                               (e.g. the stats pages). Only live screens cost periodic wake-ups. */
};

/** @} */
