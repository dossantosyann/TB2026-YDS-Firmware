#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    esp_err_t (*open)(int sample_rate, int channels);
    esp_err_t (*write)(const int16_t *buf, size_t samples);
    esp_err_t (*set_volume)(uint8_t volume);
    void      (*close)(void);
} audio_sink_t;
