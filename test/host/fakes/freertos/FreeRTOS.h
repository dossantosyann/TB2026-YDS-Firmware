/* Host-test fake: just enough FreeRTOS for the UI layer (navigator refresh ticks). */
#pragma once
#include <stdint.h>

typedef uint32_t TickType_t;

#define portMAX_DELAY     ((TickType_t)0xFFFFFFFFu)
#define pdMS_TO_TICKS(ms) ((TickType_t)((ms) / 10u))   /* 100 Hz tick, matches sdkconfig */
