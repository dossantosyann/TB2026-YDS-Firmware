/* Host unit test for the playlist navigation logic.
   Drives playlist.c over a FAKE storage index (count/path/name served from arrays here, no SD,
   no ESP-IDF). Checks: linear next/prev/select with wrap, repeat OFF end-of-list, not-ready and
   empty failing loud, select out of range, rescan clamp, and shuffle covering every track once
   while keeping the current track. */
#include "playlist.h"
#include "storage.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ---- fake storage index ------------------------------------------------------------------ */
static char   g_name[16][8];
static char   g_path[16][16];
static size_t g_count;
static bool   g_ready;

static void fake_set(size_t n, bool ready)
{
    assert(n <= 16);
    g_count = n;
    g_ready = ready;
    for (size_t i = 0; i < n; i++) {
        snprintf(g_name[i], sizeof g_name[i], "t%zu", i);
        snprintf(g_path[i], sizeof g_path[i], "/sd/t%zu", i);
    }
}

bool        storage_ready(void)             { return g_ready; }
size_t      storage_count(void)             { return g_count; }
const char *storage_track_path(size_t i)    { return i < g_count ? g_path[i] : NULL; }
const char *storage_track_name(size_t i)    { return i < g_count ? g_name[i] : NULL; }

/* Assert the resolved track is storage index @p idx with the matching path/name. */
static void expect(const playlist_track_t *t, size_t idx)
{
    char p[16], n[8];
    snprintf(p, sizeof p, "/sd/t%zu", idx);
    snprintf(n, sizeof n, "t%zu", idx);
    assert(t->index == idx);
    assert(strcmp(t->path, p) == 0);
    assert(strcmp(t->name, n) == 0);
}

/* ---- tests ------------------------------------------------------------------------------- */
static void test_linear_and_wrap(void)
{
    fake_set(5, true);
    playlist_set_shuffle(false);
    playlist_set_repeat(PLAYLIST_REPEAT_ALL);
    assert(playlist_sync() == ESP_OK);

    playlist_track_t t;
    assert(playlist_current(&t) == ESP_OK);
    expect(&t, 0);

    for (size_t i = 1; i < 5; i++) {
        assert(playlist_next(&t) == ESP_OK);
        expect(&t, i);
    }
    /* next on the last wraps to the first (repeat ALL). */
    assert(playlist_next(&t) == ESP_OK);
    expect(&t, 0);
    /* prev on the first wraps to the last. */
    assert(playlist_prev(&t) == ESP_OK);
    expect(&t, 4);

    assert(playlist_select(2, &t) == ESP_OK);
    expect(&t, 2);

    /* out param may be NULL: navigation still happens. */
    assert(playlist_next(NULL) == ESP_OK);
    assert(playlist_current(&t) == ESP_OK);
    expect(&t, 3);

    printf("playlist: linear next/prev/select + wrap OK\n");
}

static void test_repeat_off(void)
{
    fake_set(4, true);
    playlist_set_shuffle(false);
    playlist_set_repeat(PLAYLIST_REPEAT_OFF);
    assert(playlist_sync() == ESP_OK);

    playlist_track_t t;
    assert(playlist_select(3, &t) == ESP_OK);
    /* end of list: no next, position unchanged. */
    assert(playlist_next(&t) == ESP_ERR_NOT_FOUND);
    assert(playlist_current(&t) == ESP_OK);
    expect(&t, 3);

    assert(playlist_select(0, &t) == ESP_OK);
    assert(playlist_prev(&t) == ESP_ERR_NOT_FOUND);
    assert(playlist_current(&t) == ESP_OK);
    expect(&t, 0);

    playlist_set_repeat(PLAYLIST_REPEAT_ALL);
    printf("playlist: repeat OFF stops at the ends OK\n");
}

static void test_not_ready(void)
{
    fake_set(3, false);
    playlist_track_t t;
    assert(playlist_sync() == ESP_ERR_INVALID_STATE);
    assert(playlist_current(&t) == ESP_ERR_INVALID_STATE);
    assert(playlist_next(&t) == ESP_ERR_INVALID_STATE);
    assert(playlist_prev(&t) == ESP_ERR_INVALID_STATE);
    assert(playlist_select(0, &t) == ESP_ERR_INVALID_STATE);
    printf("playlist: storage not ready -> INVALID_STATE OK\n");
}

static void test_empty(void)
{
    fake_set(0, true);
    playlist_track_t t;
    assert(playlist_sync() == ESP_OK);          /* binding to an empty list is legitimate */
    assert(playlist_current(&t) == ESP_ERR_NOT_FOUND);
    assert(playlist_next(&t) == ESP_ERR_NOT_FOUND);
    assert(playlist_prev(&t) == ESP_ERR_NOT_FOUND);
    printf("playlist: empty list -> NOT_FOUND OK\n");
}

static void test_select_oob(void)
{
    fake_set(3, true);
    assert(playlist_sync() == ESP_OK);
    playlist_track_t t;
    assert(playlist_select(3, &t) == ESP_ERR_INVALID_ARG);
    assert(playlist_select(99, &t) == ESP_ERR_INVALID_ARG);
    printf("playlist: select out of range -> INVALID_ARG OK\n");
}

static void test_rescan_clamp(void)
{
    fake_set(5, true);
    playlist_set_shuffle(false);
    assert(playlist_sync() == ESP_OK);

    playlist_track_t t;
    assert(playlist_select(4, &t) == ESP_OK);   /* sit on the last track */

    /* list shrinks: current is pulled to the new last track. */
    fake_set(2, true);
    assert(playlist_sync() == ESP_OK);
    assert(playlist_current(&t) == ESP_OK);
    expect(&t, 1);

    /* list grows: position is kept (clamp only lowers). */
    fake_set(6, true);
    assert(playlist_sync() == ESP_OK);
    assert(playlist_current(&t) == ESP_OK);
    expect(&t, 1);

    printf("playlist: rescan clamp OK\n");
}

static void test_shuffle(void)
{
    fake_set(6, true);
    playlist_set_shuffle(false);
    playlist_set_repeat(PLAYLIST_REPEAT_ALL);
    assert(playlist_sync() == ESP_OK);

    playlist_track_t t;
    assert(playlist_select(0, &t) == ESP_OK);

    /* enabling shuffle keeps the current track playing. */
    playlist_set_shuffle(true);
    assert(playlist_get_shuffle() == true);
    assert(playlist_current(&t) == ESP_OK);
    expect(&t, 0);

    /* one lap covers every storage index exactly once. */
    bool seen[6] = {0};
    for (size_t i = 0; i < 6; i++) {
        assert(playlist_current(&t) == ESP_OK);
        assert(t.index < 6 && !seen[t.index]);
        seen[t.index] = true;
        expect(&t, t.index);            /* path/name follow the resolved index */
        assert(playlist_next(NULL) == ESP_OK);
    }
    for (size_t i = 0; i < 6; i++) {
        assert(seen[i]);
    }

    /* disabling shuffle restores ascending order, current track preserved. */
    assert(playlist_select(0, &t) == ESP_OK);
    playlist_set_shuffle(false);
    assert(playlist_current(&t) == ESP_OK);
    expect(&t, 0);
    assert(playlist_next(&t) == ESP_OK);
    expect(&t, 1);

    printf("playlist: shuffle covers all once + keeps current OK\n");
}

int main(void)
{
    test_linear_and_wrap();
    test_repeat_off();
    test_not_ready();
    test_empty();
    test_select_oob();
    test_rescan_clamp();
    test_shuffle();
    printf("playlist: all assertions passed\n");
    return 0;
}
