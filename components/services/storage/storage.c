#include "storage.h"
#include "sdcard.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "storage";

/* One contiguous PSRAM block of fixed-size path rows: bounded, O(1) by index, qsort-able. */
static char (*s_paths)[STORAGE_PATH_MAX];
static size_t s_count;
static bool   s_mounted;
static bool   s_scanned;

/* True when @p name ends in ".mp3" or ".wav" (case-insensitive) — what decoder_open accepts. */
static bool is_playable(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) {
        return false;
    }
    return strcasecmp(dot, ".mp3") == 0 || strcasecmp(dot, ".wav") == 0;
}

static int cmp_paths(const void *a, const void *b)
{
    return strcasecmp((const char *)a, (const char *)b);
}

esp_err_t storage_scan_dir(const char *root)
{
    if (!s_paths) {
        s_paths = heap_caps_malloc(sizeof(*s_paths) * STORAGE_MAX_TRACKS, MALLOC_CAP_SPIRAM);
        if (!s_paths) {
            ESP_LOGE(TAG, "out of PSRAM for the track index");
            return ESP_ERR_NO_MEM;
        }
    }

    DIR *dir = opendir(root);
    if (!dir) {
        ESP_LOGE(TAG, "cannot open directory '%s'", root);
        s_scanned = false;
        s_count = 0;
        return ESP_FAIL;
    }

    s_count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!is_playable(ent->d_name)) {
            continue;
        }
        if (s_count >= STORAGE_MAX_TRACKS) {
            ESP_LOGE(TAG, "more than %d tracks on the card: index overflow, refusing to truncate",
                     STORAGE_MAX_TRACKS);
            closedir(dir);
            s_count = 0;
            s_scanned = false;
            return ESP_ERR_NO_MEM;
        }
        int n = snprintf(s_paths[s_count], STORAGE_PATH_MAX, "%s/%s", root, ent->d_name);
        if (n < 0 || n >= STORAGE_PATH_MAX) {
            ESP_LOGW(TAG, "path too long, skipping: %s/%s", root, ent->d_name);
            continue;
        }
        s_count++;
    }
    closedir(dir);

    qsort(s_paths, s_count, sizeof(*s_paths), cmp_paths);
    s_scanned = true;

    if (s_count == 0) {
        ESP_LOGW(TAG, "no playable tracks found in '%s'", root);
    } else {
        ESP_LOGI(TAG, "indexed %u tracks from '%s'", (unsigned)s_count, root);
    }
    return ESP_OK;
}

esp_err_t storage_init(void)
{
    if (s_mounted && s_scanned) {
        return ESP_OK;
    }
    if (!sdcard_present()) {
        ESP_LOGE(TAG, "no SD card inserted");
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t err = sdcard_mount();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sdcard_mount failed: %d", err);
        return err;
    }
    s_mounted = true;
    return storage_scan_dir(SDCARD_MOUNT_POINT);
}

esp_err_t storage_rescan(void)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "rescan requested but card is not mounted");
        return ESP_ERR_INVALID_STATE;
    }
    return storage_scan_dir(SDCARD_MOUNT_POINT);
}

bool storage_ready(void)
{
    return s_mounted && s_scanned;
}

size_t storage_count(void)
{
    return s_scanned ? s_count : 0;
}

const char *storage_track_path(size_t index)
{
    if (!s_scanned || index >= s_count) {
        return NULL;
    }
    return s_paths[index];
}

const char *storage_track_name(size_t index)
{
    const char *path = storage_track_path(index);
    if (!path) {
        return NULL;
    }
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

esp_err_t storage_deinit(void)
{
    esp_err_t err = ESP_OK;
    if (s_mounted) {
        err = sdcard_unmount();
        s_mounted = false;
    }
    if (s_paths) {
        heap_caps_free(s_paths);
        s_paths = NULL;
    }
    s_count = 0;
    s_scanned = false;
    return err;
}
