// Screen stack router

#pragma once

#include "screen.h"

void navigator_push(screen_t *screen);
void navigator_pop(void);
void navigator_tick(ui_event_t event);
