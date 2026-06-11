#pragma once

#include "esp_err.h"

esp_err_t sdcard_mount(const char *mount_point);
esp_err_t sdcard_unmount(void);
