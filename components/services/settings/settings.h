#pragma once

#include "esp_err.h"

typedef enum {
    AUDIO_OUTPUT_JACK,
    AUDIO_OUTPUT_BLUETOOTH,
} audio_output_t;

esp_err_t      settings_init(void);
audio_output_t settings_get_output(void);
esp_err_t      settings_set_output(audio_output_t output);
