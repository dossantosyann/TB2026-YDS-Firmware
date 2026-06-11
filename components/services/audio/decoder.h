#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int  (*decode)(const uint8_t *in, size_t in_len, int16_t *out, size_t *out_samples);
    void (*reset)(void);
} audio_decoder_t;
