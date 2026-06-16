#pragma once
/* Minimal host stand-in for ESP-IDF's esp_err.h, so pure-logic component code
   (gfx, display_oled headers) compiles off-target for unit tests. */
#include <stdint.h>
#include <stddef.h>

typedef int esp_err_t;
#define ESP_OK 0
