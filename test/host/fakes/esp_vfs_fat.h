#pragma once
/* Minimal host stand-in: storage_get_usage() is never called in host tests. */
#include "esp_err.h"
#include <stdint.h>

static inline esp_err_t esp_vfs_fat_info(const char *path, uint64_t *total, uint64_t *free_b)
{
    (void)path; (void)total; (void)free_b;
    return ESP_ERR_NOT_SUPPORTED;
}
