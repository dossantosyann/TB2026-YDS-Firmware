#include "storage_screen.h"
#include "navigator.h"
#include "gfx.h"
#include "status_bar.h"
#include "storage.h"
#include "sdcard.h"
#include "player.h"
#include "playlist.h"

#include <ctype.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

/* ---- layout ---------------------------------------------------------------- */

#define FB_CONTENT_Y  STATUS_BAR_H          /* 16 */
#define FB_ROW_H      12
#define FB_VISIBLE    12                     /* rows × 12px = 144px of content */
#define FB_ARROW_X    4
#define FB_TEXT_X     14
#define FB_DIVIDER_Y  161
#define FB_PATH_Y     163

/* ---- data ------------------------------------------------------------------ */

#define FB_MAX_ENTRIES 64
#define FB_NAME_MAX    128

typedef struct {
    char name[FB_NAME_MAX];
    bool is_dir;
} fb_entry_t;

typedef enum { FB_BROWSE, FB_ACTION, FB_CONFIRM } fb_state_t;

static char       s_path[STORAGE_PATH_MAX];
static fb_entry_t s_entries[FB_MAX_ENTRIES];
static int        s_count;
static int        s_sel;
static int        s_top;
static fb_state_t s_state;
static int        s_action_sel;    /* BROWSE: unused; ACTION: 0=Play,1=Delete; CONFIRM: 0=Yes,1=No */
static int        s_marquee_offset; /* char offset into the selected item's name */
static int        s_marquee_tick;   /* ticks since last offset change */

/* ---- helpers --------------------------------------------------------------- */

static bool is_audio(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot && (strcasecmp(dot, ".mp3") == 0 || strcasecmp(dot, ".wav") == 0);
}

static int cmp_entries(const void *a, const void *b)
{
    const fb_entry_t *ea = (const fb_entry_t *)a;
    const fb_entry_t *eb = (const fb_entry_t *)b;
    if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
    return strcasecmp(ea->name, eb->name);
}

static void reset_marquee(void)
{
    s_marquee_offset = 0;
    s_marquee_tick   = 0;
}

static void scan_current(void)
{
    s_count = 0;
    s_sel   = 0;
    s_top   = 0;
    reset_marquee();

    DIR *dir = opendir(s_path);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && s_count < FB_MAX_ENTRIES) {
        if (ent->d_name[0] == '.') continue;
        bool dir_entry = (ent->d_type == DT_DIR);
        bool audio_entry = (ent->d_type == DT_REG && is_audio(ent->d_name));
        if (!dir_entry && !audio_entry) continue;
        fb_entry_t *e = &s_entries[s_count++];
        strlcpy(e->name, ent->d_name, FB_NAME_MAX);
        e->is_dir = dir_entry;
    }
    closedir(dir);
    qsort(s_entries, s_count, sizeof(fb_entry_t), cmp_entries);
}

static void enter_dir(const char *name)
{
    strlcat(s_path, "/", sizeof s_path);
    strlcat(s_path, name, sizeof s_path);
    scan_current();
}

static void leave_dir(void)
{
    if (strcmp(s_path, SDCARD_MOUNT_POINT) == 0) {
        navigator_pop();
        return;
    }
    char *slash = strrchr(s_path, '/');
    if (slash) *slash = '\0';
    scan_current();
}

/* Build s_path + "/" + name into dst[dst_sz]. Returns false if it wouldn't fit. */
static bool build_full_path(char *dst, size_t dst_sz, const char *name)
{
    size_t plen = strlen(s_path);
    size_t nlen = strlen(name);
    if (plen + 1 + nlen >= dst_sz) return false;
    memcpy(dst, s_path, plen);
    dst[plen] = '/';
    memcpy(dst + plen + 1, name, nlen + 1);
    return true;
}

static void play_selected(void)
{
    char full[STORAGE_PATH_MAX];
    if (!build_full_path(full, sizeof full, s_entries[s_sel].name)) return;
    size_t n = storage_count();
    for (size_t i = 0; i < n; i++) {
        if (strcmp(storage_track_path(i), full) == 0) {
            player_play(i);
            return;
        }
    }
}

static void delete_selected(void)
{
    char full[STORAGE_PATH_MAX];
    if (!build_full_path(full, sizeof full, s_entries[s_sel].name)) return;
    remove(full);
    storage_rescan();
    playlist_sync();
    scan_current();
    s_state = FB_BROWSE;
}

static void adjust_scroll(void)
{
    if (s_sel < s_top)                    s_top = s_sel;
    else if (s_sel >= s_top + FB_VISIBLE) s_top = s_sel - FB_VISIBLE + 1;
    reset_marquee();
}

/* Advance the marquee one tick (called once per render from FB_BROWSE). */
#define MARQUEE_PAUSE 2   /* ticks to hold at each end (2 × 250 ms = 500 ms) */

static void marquee_step(int name_len, int max_chars)
{
    int max_off = name_len - max_chars;
    if (max_off <= 0) return;

    s_marquee_tick++;

    bool at_end = (s_marquee_offset >= max_off);
    bool at_start = (s_marquee_offset == 0);

    if ((at_start || at_end) && s_marquee_tick <= MARQUEE_PAUSE) return; /* hold */

    if (at_end) {
        s_marquee_offset = 0;
    } else {
        s_marquee_offset++;
    }
    s_marquee_tick = 0;
}

/* ---- name parsing ---------------------------------------------------------- */

/* Returns the character length of a leading numeric prefix, e.g.:
     "01 - Song.mp3"  → 5  ("01 - ")
     "1. Song.mp3"    → 3  ("1. ")
     "Song.mp3"       → 0  (no leading digit)
   After the digits we consume up to 3 separator chars in { ' ', '-', '.', '_' }. */
static int name_prefix_len(const char *name)
{
    int i = 0;
    while (isdigit((unsigned char)name[i])) i++;
    if (i == 0) return 0;
    int j = i;
    while (j < i + 3 && (name[j] == ' ' || name[j] == '-' || name[j] == '.' || name[j] == '_'))
        j++;
    return j;
}

/* ---- truncation helpers ---------------------------------------------------- */

/* Write at most max_chars visible chars of src into dst (size must be max_chars+1).
   Left-truncates with "..." if src is longer. */
static void truncate_left(char *dst, size_t dst_sz, const char *src, int max_chars)
{
    int len = (int)strlen(src);
    if (len <= max_chars) {
        snprintf(dst, dst_sz, "%s", src);
    } else {
        snprintf(dst, dst_sz, "...%s", src + len - (max_chars - 3));
    }
}

/* Right-truncates with "..." */
static void truncate_right(char *dst, size_t dst_sz, const char *src, int max_chars)
{
    int len = (int)strlen(src);
    if (len <= max_chars) {
        snprintf(dst, dst_sz, "%s", src);
    } else {
        snprintf(dst, dst_sz, "%.*s...", max_chars - 3, src);
    }
}

/* ---- render: browse -------------------------------------------------------- */

static void draw_path_bar(void)
{
    gfx_fill_rect(0, FB_DIVIDER_Y, GFX_W, 1, gfx_rgb(50, 50, 50));
    int max_chars = GFX_W / GFX_CHAR_W; /* 29 */
    char buf[30];
    truncate_left(buf, sizeof buf, s_path, max_chars);
    gfx_draw_text(0, FB_PATH_Y, buf, gfx_rgb(120, 120, 120), 1);
}

static void render_browse(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);

    if (!storage_ready()) {
        gfx_draw_text(FB_TEXT_X, FB_CONTENT_Y + 16, "No SD card", gfx_rgb(180, 80, 80), 1);
        draw_path_bar();
        return;
    }

    if (s_count == 0) {
        gfx_draw_text(FB_TEXT_X, FB_CONTENT_Y + 16, "Empty folder", gfx_rgb(120, 120, 120), 1);
        draw_path_bar();
        return;
    }

    /* max chars that fit in the name column */
    int max_name = (GFX_W - FB_TEXT_X) / GFX_CHAR_W; /* (176-14)/6 = 27 */

    for (int i = 0; i < FB_VISIBLE && (s_top + i) < s_count; i++) {
        int idx = s_top + i;
        int y   = FB_CONTENT_Y + i * FB_ROW_H;
        bool sel = (idx == s_sel);

        if (sel) gfx_fill_rect(0, y, GFX_W, FB_ROW_H, GFX_WHITE);

        gfx_color_t fg = sel ? GFX_BLACK : GFX_WHITE;
        gfx_color_t dir_color = sel ? GFX_BLACK : gfx_rgb(100, 180, 255);

        fb_entry_t *e = &s_entries[idx];

        if (e->is_dir) {
            gfx_draw_text(FB_ARROW_X, y + 2, "/", dir_color, 1);
        }

        gfx_color_t name_color = e->is_dir ? dir_color : fg;

        /* Split "01 - " prefix (pinned) from the scrollable body. */
        int prefix   = name_prefix_len(e->name);
        int prefix_x = FB_TEXT_X + prefix * GFX_CHAR_W;
        int body_max = max_name - prefix;
        const char *body = e->name + prefix;
        int body_len = (int)strlen(body);

        if (prefix > 0) {
            char pre[8];
            strlcpy(pre, e->name, (size_t)(prefix + 1));
            gfx_draw_text(FB_TEXT_X, y + 2, pre, name_color, 1);
        }

        if (sel && body_len > body_max) {
            /* marquee on the body only; canvas clips the right overflow */
            gfx_draw_text(prefix_x, y + 2, body + s_marquee_offset, name_color, 1);
            marquee_step(body_len, body_max);
        } else {
            char label[28];
            truncate_right(label, sizeof label, body, body_max);
            gfx_draw_text(prefix_x, y + 2, label, name_color, 1);
        }
    }

    draw_path_bar();
}

/* ---- render: action popup -------------------------------------------------- */

#define POPUP_X  20
#define POPUP_Y  56
#define POPUP_W  136
#define POPUP_H  56

static const char *const s_action_labels[] = { "Play", "Delete" };
#define N_ACTIONS 2

static void render_action(screen_t *self)
{
    render_browse(self);

    gfx_fill_rect(POPUP_X - 2, POPUP_Y - 2, POPUP_W + 4, POPUP_H + 4, GFX_BLACK);
    gfx_draw_rect(POPUP_X, POPUP_Y, POPUP_W, POPUP_H, GFX_WHITE);

    int name_max = (POPUP_W - 8) / GFX_CHAR_W;
    char label[24];
    truncate_right(label, sizeof label, s_entries[s_sel].name, name_max);
    gfx_draw_text(POPUP_X + 4, POPUP_Y + 4, label, gfx_rgb(200, 200, 200), 1);
    gfx_fill_rect(POPUP_X + 4, POPUP_Y + 14, POPUP_W - 8, 1, gfx_rgb(70, 70, 70));

    for (int i = 0; i < N_ACTIONS; i++) {
        int y = POPUP_Y + 20 + i * 16;
        bool sel = (i == s_action_sel);
        if (sel) {
            gfx_fill_rect(POPUP_X + 4, y - 1, POPUP_W - 8, 14, GFX_WHITE);
            gfx_draw_text(POPUP_X + 10, y + 2, s_action_labels[i], GFX_BLACK, 1);
        } else {
            gfx_draw_text(POPUP_X + 10, y + 2, s_action_labels[i], GFX_WHITE, 1);
        }
    }
}

/* ---- render: confirm popup ------------------------------------------------- */

static void render_confirm(screen_t *self)
{
    render_browse(self);

    gfx_fill_rect(POPUP_X - 2, POPUP_Y - 2, POPUP_W + 4, POPUP_H + 4, GFX_BLACK);
    gfx_draw_rect(POPUP_X, POPUP_Y, POPUP_W, POPUP_H, gfx_rgb(220, 60, 60));
    gfx_draw_text(POPUP_X + 4, POPUP_Y + 4, "Delete?", GFX_WHITE, 1);

    int name_max = (POPUP_W - 8) / GFX_CHAR_W;
    char label[24];
    truncate_right(label, sizeof label, s_entries[s_sel].name, name_max);
    gfx_draw_text(POPUP_X + 4, POPUP_Y + 14, label, gfx_rgb(200, 200, 200), 1);

    const char *const opts[] = { "Yes", "No" };
    for (int i = 0; i < 2; i++) {
        int bx = POPUP_X + 16 + i * 56;
        int by = POPUP_Y + 36;
        bool sel = (i == s_action_sel);
        if (sel) {
            gfx_fill_rect(bx - 4, by - 2, 40, 14, GFX_WHITE);
            gfx_draw_text(bx + 4, by + 2, opts[i], GFX_BLACK, 1);
        } else {
            gfx_draw_rect(bx - 4, by - 2, 40, 14, gfx_rgb(140, 140, 140));
            gfx_draw_text(bx + 4, by + 2, opts[i], GFX_WHITE, 1);
        }
    }
}

/* ---- screen vtable --------------------------------------------------------- */

static void on_enter(screen_t *self)
{
    (void)self;
    strlcpy(s_path, SDCARD_MOUNT_POINT, sizeof s_path);
    s_state      = FB_BROWSE;
    s_action_sel = 0;
    scan_current();
}

static void fb_on_exit(screen_t *self) { (void)self; }

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;

    switch (s_state) {

    case FB_BROWSE:
        switch (ev) {
        case UI_EVENT_UP:
            if (s_sel > 0) { s_sel--; adjust_scroll(); }
            break;
        case UI_EVENT_DOWN:
            if (s_sel < s_count - 1) { s_sel++; adjust_scroll(); }
            break;
        case UI_EVENT_SELECT:
            if (s_count == 0) break;
            if (s_entries[s_sel].is_dir) {
                enter_dir(s_entries[s_sel].name);
            } else {
                s_state      = FB_ACTION;
                s_action_sel = 0;
            }
            break;
        case UI_EVENT_BACK:
            leave_dir();
            break;
        default: break;
        }
        break;

    case FB_ACTION:
        switch (ev) {
        case UI_EVENT_UP:
        case UI_EVENT_DOWN:
            s_action_sel = 1 - s_action_sel;
            break;
        case UI_EVENT_SELECT:
            if (s_action_sel == 0) {
                play_selected();
                s_state = FB_BROWSE;
            } else {
                s_state      = FB_CONFIRM;
                s_action_sel = 1; /* default: No */
            }
            break;
        case UI_EVENT_BACK:
            s_state = FB_BROWSE;
            break;
        default: break;
        }
        break;

    case FB_CONFIRM:
        switch (ev) {
        case UI_EVENT_LEFT:
        case UI_EVENT_RIGHT:
            s_action_sel = 1 - s_action_sel;
            break;
        case UI_EVENT_SELECT:
            if (s_action_sel == 0) {
                delete_selected(); /* sets s_state = FB_BROWSE */
            } else {
                s_state      = FB_ACTION;
                s_action_sel = 1;
            }
            break;
        case UI_EVENT_BACK:
            s_state      = FB_ACTION;
            s_action_sel = 1;
            break;
        default: break;
        }
        break;
    }
}

static void render(screen_t *self)
{
    switch (s_state) {
    case FB_BROWSE:  render_browse(self);  break;
    case FB_ACTION:  render_action(self);  break;
    case FB_CONFIRM: render_confirm(self); break;
    }
}

screen_t *storage_screen(void)
{
    static screen_t s = {
        .on_enter     = on_enter,
        .on_exit      = fb_on_exit,
        .handle_input = handle_input,
        .render       = render,
        .refresh_ms   = 250,
    };
    return &s;
}
