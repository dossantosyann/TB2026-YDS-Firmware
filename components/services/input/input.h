#pragma once

#include "esp_err.h"

typedef enum {
    INPUT_BTN_PLAY,
    INPUT_BTN_NEXT,
    INPUT_BTN_PREV,
    INPUT_BTN_VOL_UP,
    INPUT_BTN_VOL_DOWN,
    INPUT_POT_CHANGED,
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    int                value;
} input_event_t;

typedef void (*input_event_cb_t)(const input_event_t *event, void *ctx);

esp_err_t input_init(input_event_cb_t cb, void *ctx);
