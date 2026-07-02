#include "playlist.h"
#include "storage.h"
#include "esp_log.h"

#include <stdlib.h>

static const char *TAG = "playlist";

/* Play order: order[pos] is the storage index of the track at play position pos. Identity when
   shuffle is off, a permutation of [0, s_count) when on. s_count mirrors storage_count() as of
   the last sync, so a stale storage index never leaks into navigation. */
static uint16_t order[STORAGE_MAX_TRACKS];
static size_t   s_count;
static size_t   s_pos;
static playlist_repeat_t s_repeat = PLAYLIST_REPEAT_ALL;
static bool     s_shuffle;

static void order_identity(void)
{
    for (size_t i = 0; i < s_count; i++) {
        order[i] = (uint16_t)i;
    }
}

/* Fisher-Yates over the current order contents: every track appears exactly once. */
static void order_shuffle(void)
{
    for (size_t i = s_count; i-- > 1; ) {
        size_t j = (size_t)rand() % (i + 1);
        uint16_t t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
}

/* Position of storage index @p idx in the current order, or s_count if absent. */
static size_t pos_of(size_t idx)
{
    for (size_t p = 0; p < s_count; p++) {
        if (order[p] == idx) {
            return p;
        }
    }
    return s_count;
}

static esp_err_t fill_current(playlist_track_t *out)
{
    if (!storage_ready()) {
        ESP_LOGE(TAG, "storage not ready");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_count == 0) {
        ESP_LOGW(TAG, "no tracks");
        return ESP_ERR_NOT_FOUND;
    }
    if (out) {
        size_t idx = order[s_pos];
        out->index = idx;
        out->path = storage_track_path(idx);
        out->name = storage_track_name(idx);
    }
    return ESP_OK;
}

esp_err_t playlist_sync(void)
{
    if (!storage_ready()) {
        ESP_LOGE(TAG, "sync: storage not ready");
        return ESP_ERR_INVALID_STATE;
    }
    s_count = storage_count();
    order_identity();
    if (s_shuffle) {
        order_shuffle();
    }
    /* Clamp: keep the position when the list grew, pull it to the last track when it shrank. */
    if (s_count == 0) {
        s_pos = 0;
    } else if (s_pos >= s_count) {
        s_pos = s_count - 1;
    }
    return ESP_OK;
}

esp_err_t playlist_current(playlist_track_t *out)
{
    return fill_current(out);
}

esp_err_t playlist_next(playlist_track_t *out)
{
    if (!storage_ready()) {
        ESP_LOGE(TAG, "next: storage not ready");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_count == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (s_pos + 1 >= s_count) {
        if (s_repeat == PLAYLIST_REPEAT_OFF) {
            return ESP_ERR_NOT_FOUND;
        }
        s_pos = 0;
    } else {
        s_pos++;
    }
    return fill_current(out);
}

esp_err_t playlist_prev(playlist_track_t *out)
{
    if (!storage_ready()) {
        ESP_LOGE(TAG, "prev: storage not ready");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_count == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (s_pos == 0) {
        if (s_repeat == PLAYLIST_REPEAT_OFF) {
            return ESP_ERR_NOT_FOUND;
        }
        s_pos = s_count - 1;
    } else {
        s_pos--;
    }
    return fill_current(out);
}

esp_err_t playlist_select(size_t index, playlist_track_t *out)
{
    if (!storage_ready()) {
        ESP_LOGE(TAG, "select: storage not ready");
        return ESP_ERR_INVALID_STATE;
    }
    if (index >= s_count) {
        ESP_LOGE(TAG, "select: index %zu out of range (%zu)", index, s_count);
        return ESP_ERR_INVALID_ARG;
    }
    s_pos = pos_of(index);
    return fill_current(out);
}

esp_err_t playlist_random(playlist_track_t *out)
{
    if (!storage_ready()) {
        ESP_LOGE(TAG, "random: storage not ready");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_count == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    order_shuffle();   /* fresh permutation over the current order contents */
    s_pos = 0;
    return fill_current(out);
}

/* ---- queue view/edit --------------------------------------------------- */
/* Row r maps to play order position s_pos + r (row 0 = current track). Edits stay strictly
   above s_pos (upcoming region), so the current track and its playback are never disturbed. */

size_t playlist_queue_len(void)
{
    if (!storage_ready() || s_count == 0) {
        return 0;
    }
    return s_count - s_pos;
}

esp_err_t playlist_queue_at(size_t row, playlist_track_t *out)
{
    if (!storage_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t pos = s_pos + row;
    if (s_count == 0 || pos >= s_count) {
        return ESP_ERR_NOT_FOUND;
    }
    if (out) {
        size_t idx = order[pos];
        out->index = idx;
        out->path  = storage_track_path(idx);
        out->name  = storage_track_name(idx);
    }
    return ESP_OK;
}

esp_err_t playlist_queue_move_up(size_t row)
{
    if (!storage_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    /* row < 2 would move the current track or push a track above it. */
    if (row < 2 || s_pos + row >= s_count) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t p = s_pos + row;
    uint16_t t = order[p];
    order[p] = order[p - 1];
    order[p - 1] = t;
    return ESP_OK;
}

esp_err_t playlist_queue_move_down(size_t row)
{
    if (!storage_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (row < 1 || s_pos + row + 1 >= s_count) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t p = s_pos + row;
    uint16_t t = order[p];
    order[p] = order[p + 1];
    order[p + 1] = t;
    return ESP_OK;
}

esp_err_t playlist_queue_remove(size_t row)
{
    if (!storage_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (row < 1 || s_pos + row >= s_count) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t p = s_pos + row;
    for (size_t i = p; i + 1 < s_count; i++) {
        order[i] = order[i + 1];
    }
    s_count--;
    return ESP_OK;
}

void playlist_set_repeat(playlist_repeat_t mode)
{
    s_repeat = mode;
}

playlist_repeat_t playlist_get_repeat(void)
{
    return s_repeat;
}

void playlist_set_shuffle(bool on)
{
    if (on == s_shuffle) {
        return;
    }
    s_shuffle = on;
    if (!storage_ready() || s_count == 0) {
        return;  /* applied on the next playlist_sync() */
    }
    /* Rebuild without interrupting the current track: rebuild the order, then point pos at it. */
    size_t cur = order[s_pos];
    order_identity();
    if (s_shuffle) {
        order_shuffle();
    }
    s_pos = pos_of(cur);
}

bool playlist_get_shuffle(void)
{
    return s_shuffle;
}
