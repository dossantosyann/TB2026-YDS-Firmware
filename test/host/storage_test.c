/* Host unit test for the storage track-enumeration logic.
   Exercises storage_scan_dir() on a temporary folder of fake files: extension filter
   (case-insensitive), sort order, well-formed paths, and the overflow guard.
   No ESP-IDF, no SD card. The sdcard_* driver is stubbed (never called here). */
#include "storage.h"
#include "sdcard.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* sdcard driver stubs: storage.c references these in init/deinit; this test never calls
   those paths, but the linker still needs the symbols. */
esp_err_t sdcard_mount(void)   { return ESP_OK; }
esp_err_t sdcard_unmount(void) { return ESP_OK; }
bool      sdcard_present(void) { return false; }

static void touch(const char *root, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", root, name);
    FILE *f = fopen(path, "w");
    assert(f);
    fclose(f);
}

static void cleanup(const char *root)
{
    DIR *d = opendir(root);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
                continue;
            }
            char path[512];
            snprintf(path, sizeof path, "%s/%s", root, e->d_name);
            unlink(path);
        }
        closedir(d);
    }
    rmdir(root);
}

static void test_filter_and_sort(void)
{
    char root[] = "/tmp/storage_test_XXXXXX";
    assert(mkdtemp(root));

    /* Mix of playable files (varied case), and non-audio that must be dropped. */
    touch(root, "beta.mp3");
    touch(root, "Alpha.MP3");
    touch(root, "gamma.wav");
    touch(root, "delta.Wav");
    touch(root, "readme.txt");      /* wrong extension */
    touch(root, "noext");           /* no extension */
    touch(root, "song.mp3.bak");    /* extension is .bak */

    assert(storage_scan_dir(root) == ESP_OK);

    /* Only the four audio files, regardless of extension case. */
    assert(storage_count() == 4);

    /* Sorted case-insensitively for deterministic next/prev. */
    const char *expect[] = { "Alpha.MP3", "beta.mp3", "delta.Wav", "gamma.wav" };
    for (size_t i = 0; i < 4; i++) {
        assert(strcmp(storage_track_name(i), expect[i]) == 0);

        /* Path is "<root>/<name>", well-formed for decoder_open(). */
        char want[512];
        snprintf(want, sizeof want, "%s/%s", root, expect[i]);
        assert(strcmp(storage_track_path(i), want) == 0);
    }

    /* Out-of-range access is NULL, not a crash. */
    assert(storage_track_path(4) == NULL);
    assert(storage_track_name(4) == NULL);

    cleanup(root);
    printf("storage: filter + sort + paths OK\n");
}

static void test_empty_dir(void)
{
    char root[] = "/tmp/storage_test_XXXXXX";
    assert(mkdtemp(root));

    touch(root, "cover.jpg");   /* present but not playable */

    /* Empty of audio is a legitimate state: OK, zero tracks (not an error). */
    assert(storage_scan_dir(root) == ESP_OK);
    assert(storage_count() == 0);
    assert(storage_track_path(0) == NULL);

    cleanup(root);
    printf("storage: empty card -> OK, 0 tracks\n");
}

static void test_overflow(void)
{
    char root[] = "/tmp/storage_test_XXXXXX";
    assert(mkdtemp(root));

    for (int i = 0; i < STORAGE_MAX_TRACKS + 1; i++) {
        char name[32];
        snprintf(name, sizeof name, "t%04d.mp3", i);
        touch(root, name);
    }

    /* Too many tracks fails loud rather than truncating silently. */
    assert(storage_scan_dir(root) == ESP_ERR_NO_MEM);
    assert(storage_count() == 0);

    cleanup(root);
    printf("storage: overflow -> ESP_ERR_NO_MEM, no silent truncation\n");
}

int main(void)
{
    test_filter_and_sort();
    test_empty_dir();
    test_overflow();
    printf("storage: all assertions passed\n");
    return 0;
}
