/* Host-test fake: nothing from task.h is exercised by the UI-layer tests, except
   vTaskDelay for the screenshot tool (fuel_gauge_debug.c's reload path, never actually run). */
#pragma once
#include "FreeRTOS.h"

static inline void vTaskDelay(TickType_t ticks) { (void)ticks; }
