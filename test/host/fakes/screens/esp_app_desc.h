#pragma once
/* Minimal host stand-in for ESP-IDF's esp_app_desc.h: stats_screen.c reads only ->version. */

typedef struct {
    const char *version;
} esp_app_desc_t;

const esp_app_desc_t *esp_app_get_description(void);
