#pragma once

#include "screen.h"

typedef struct {
    const char *label;
    void       (*action)(void);
    screen_t   *navigate_to;
} menu_item_t;
