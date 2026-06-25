/**
 * @file menu_screen.h
 * @brief Generic vertical-list menu screen (a concrete screen_t).
 */
#pragma once

#include "screen.h"
#include "menu_item.h"

/**
 * @brief A scrollable list menu.
 *
 * Embeds screen_t as its first member, so a menu_screen_t* doubles as a screen_t*.
 * UP/DOWN move the selection, SELECT follows the item's navigate_to/action, BACK pops.
 */
typedef struct {
    screen_t          base;        /**< Screen vtable; must stay first for the cast. */
    const menu_item_t *items;      /**< Item array, owned by the caller. */
    int               item_count;  /**< Number of entries in items. */
    int               selected;    /**< Index of the highlighted row. */
} menu_screen_t;

/**
 * @brief Initialize a menu screen: wire the screen vtable and store the items.
 *
 * @param ms     Menu screen to initialize.
 * @param items  Item array; must outlive the screen (not copied).
 * @param count  Number of entries in @p items.
 */
void menu_screen_init(menu_screen_t *ms, const menu_item_t *items, int count);
