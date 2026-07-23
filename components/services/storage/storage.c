/**
 * @file storage.c
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "storage.h"
#include "sdcard.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_vfs_fat.h"

#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "storage";

/* One contiguous PSRAM block of fixed-size path rows: bounded, O(1) by index, qsort-able. */
static char (*s_paths)[STORAGE_PATH_MAX];
static size_t          s_count;
static volatile bool   s_mounted;
static volatile bool   s_scanned;
static uint64_t        s_total_bytes;
static uint64_t        s_used_bytes;

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

/* Recursive scan core: walk @p dir_path, add playable files to s_paths[], recurse into
   sub-directories. s_count is the shared write cursor across all recursive calls. */
static esp_err_t scan_recursive(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (!dir) {
        ESP_LOGW(TAG, "cannot open '%s', skipping", dir_path);
        return ESP_OK;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;  /* skip ., .., hidden dirs, ._AppleDouble files */

        char child[STORAGE_PATH_MAX];
        int n = snprintf(child, sizeof child, "%s/%s", dir_path, ent->d_name);
        if (n < 0 || n >= (int)sizeof child) {
            ESP_LOGW(TAG, "path too long, skipping: %s/%s", dir_path, ent->d_name);
            continue;
        }

        if (ent->d_type == DT_DIR) {
            esp_err_t err = scan_recursive(child);
            if (err != ESP_OK) {
                closedir(dir);
                return err;
            }
        } else if (is_playable(ent->d_name)) {
            if (s_count >= STORAGE_MAX_TRACKS) {
                ESP_LOGE(TAG, "more than %d tracks: index overflow, refusing to truncate",
                         STORAGE_MAX_TRACKS);
                closedir(dir);
                return ESP_ERR_NO_MEM;
            }
            memcpy(s_paths[s_count], child, n + 1);
            s_count++;
        }
    }
    closedir(dir);
    return ESP_OK;
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

    s_count = 0;
    s_scanned = false;

    esp_err_t err = scan_recursive(root);
    if (err != ESP_OK) {
        s_count = 0;
        return err;
    }

    qsort(s_paths, s_count, sizeof(*s_paths), cmp_paths);
    s_scanned = true;

    if (s_count == 0) {
        ESP_LOGW(TAG, "no playable tracks found under '%s'", root);
    } else {
        ESP_LOGI(TAG, "indexed %u tracks under '%s'", (unsigned)s_count, root);
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
    err = storage_scan_dir(SDCARD_MOUNT_POINT);
    if (err != ESP_OK) return err;
    uint64_t total = 0, freeb = 0;
    if (esp_vfs_fat_info(SDCARD_MOUNT_POINT, &total, &freeb) == ESP_OK) {
        s_total_bytes = total;
        s_used_bytes  = total - freeb;
    }
    return ESP_OK;
}

esp_err_t storage_rescan(void)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "rescan requested but card is not mounted");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = storage_scan_dir(SDCARD_MOUNT_POINT);
    if (err != ESP_OK) return err;
    uint64_t total = 0, freeb = 0;
    if (esp_vfs_fat_info(SDCARD_MOUNT_POINT, &total, &freeb) == ESP_OK) {
        s_total_bytes = total;
        s_used_bytes  = total - freeb;
    }
    return ESP_OK;
}

esp_err_t storage_unmount(void)
{
    if (!s_mounted) {
        return ESP_OK;
    }
    s_scanned = false;
    s_count   = 0;
    s_mounted = false;
    return sdcard_unmount();
}

bool storage_ready(void)
{
    return s_mounted && s_scanned;
}

size_t storage_count(void)
{
    return s_scanned ? s_count : 0;
}

esp_err_t storage_get_usage(uint64_t *total_bytes, uint64_t *used_bytes)
{
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    if (total_bytes) *total_bytes = s_total_bytes;
    if (used_bytes)  *used_bytes  = s_used_bytes;
    return ESP_OK;
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
