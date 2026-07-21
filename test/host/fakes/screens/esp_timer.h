#pragma once
/* Minimal host stand-in for ESP-IDF's esp_timer.h: stats_screen.c reads the uptime with this. */
#include <stdint.h>

int64_t esp_timer_get_time(void);
