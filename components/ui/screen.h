// Screen interface contract

#pragma once

#include "event.h"

typedef struct screen_t screen_t;

struct screen_t {
    void (*on_enter)(screen_t *self);
    void (*on_exit)(screen_t *self);
    void (*handle_input)(screen_t *self, ui_event_t event);
    void (*render)(screen_t *self);
};
