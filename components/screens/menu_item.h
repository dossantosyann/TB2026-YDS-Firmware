/**
 * @file menu_item.h
 * @brief One entry in a menu_screen: a label plus a navigation target or action.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "screen.h"

/**
 * @brief A single menu row.
 *
 * On SELECT, navigate_to takes priority: if non-NULL the navigator pushes it,
 * otherwise action is called.
 */
typedef struct {
    const char *label;          /**< Text drawn for the row. */
    screen_t   *navigate_to;    /**< Screen pushed when selected, if non-NULL. */
    void      (*action)(void);  /**< Called when selected and navigate_to is NULL. */
} menu_item_t;
