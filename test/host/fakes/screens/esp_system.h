#pragma once
/* Minimal host stand-in for ESP-IDF's esp_system.h: just the heap/reset-reason calls
   stats_screen.c's System page reads. */
#include <stdint.h>

uint32_t esp_get_free_heap_size(void);
uint32_t esp_get_minimum_free_heap_size(void);

typedef enum {
    ESP_RST_UNKNOWN = 0,
    ESP_RST_POWERON,
    ESP_RST_EXT,
    ESP_RST_SW,
    ESP_RST_PANIC,
    ESP_RST_INT_WDT,
    ESP_RST_TASK_WDT,
    ESP_RST_WDT,
    ESP_RST_DEEPSLEEP,
    ESP_RST_BROWNOUT,
    ESP_RST_SDIO,
} esp_reset_reason_t;

esp_reset_reason_t esp_reset_reason(void);
