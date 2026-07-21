#pragma once
/* Host stand-in for drivers/sdcard/sdcard.h, used only by the screenshot tool: same
   declarations as the real driver, but SDCARD_MOUNT_POINT points at a fixture folder with a
   few real files on disk, so storage_screen.c's own opendir() has something to show. */
#include "esp_err.h"
#include <stdbool.h>

#define SDCARD_MOUNT_POINT "test/host/fixtures/sdcard"

esp_err_t sdcard_mount(void);
esp_err_t sdcard_unmount(void);
bool      sdcard_present(void);
int       sdcard_freq_khz(void);
