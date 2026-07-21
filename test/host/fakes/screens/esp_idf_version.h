#pragma once
/* Minimal host stand-in for ESP-IDF's esp_idf_version.h: stats_screen.c reads this string. */

const char *esp_get_idf_version(void);
