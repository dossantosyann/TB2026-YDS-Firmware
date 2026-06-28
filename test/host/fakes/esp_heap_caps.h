#pragma once
/* Minimal host stand-in for ESP-IDF's esp_heap_caps.h: off-target there is no PSRAM,
   so capability-tagged allocations fall back to the libc heap. */
#include <stdlib.h>
#include <stdint.h>

#define MALLOC_CAP_SPIRAM  (1 << 10)

static inline void *heap_caps_malloc(size_t size, uint32_t caps)
{
    (void)caps;
    return malloc(size);
}

static inline void heap_caps_free(void *ptr)
{
    free(ptr);
}
