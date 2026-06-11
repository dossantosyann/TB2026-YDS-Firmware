#pragma once

#include "esp_err.h"
#include <stddef.h>

#define STORAGE_MOUNT_POINT "/sdcard"

esp_err_t storage_init(void);
esp_err_t storage_list_mp3(const char *dir, char **out_paths, size_t max, size_t *out_count);
