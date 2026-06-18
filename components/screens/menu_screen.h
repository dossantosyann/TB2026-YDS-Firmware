#pragma once

#include "screen.h"
#include "menu_item.h"

typedef struct {
    screen_t         base;
    const menu_item_t *items;
    int              item_count;
    int              selected;
} menu_screen_t;

void menu_screen_init(menu_screen_t *ms, const menu_item_t *items, int count);
