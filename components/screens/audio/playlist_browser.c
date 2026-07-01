#include "playlist_browser.h"
#include "navigator.h"
#include "gfx.h"
#include "status_bar.h"
#include "playlist.h"

#include <string.h>
#include <stdio.h>

/* ---- layout ---------------------------------------------------------------- */

#define PB_CONTENT_Y  STATUS_BAR_H          /* 16 */
#define PB_ROW_H      12
#define PB_VISIBLE    12                     /* rows × 12px = 144px of content */
#define PB_TEXT_X     4

/* ---- state ----------------------------------------------------------------- */

typedef enum { PB_BROWSE, PB_ACTION } pb_state_t;

/* Action menu: 0 = move up, 1 = move down, 2 = remove. */
enum { PB_ACT_UP, PB_ACT_DOWN, PB_ACT_REMOVE, PB_N_ACTIONS };

static int        s_sel;            /* queue row (0 = current track) */
static int        s_top;            /* first visible row */
static pb_state_t s_state;
static int        s_action_sel;
static int        s_marquee_offset; /* char offset into the selected item's name */
static int        s_marquee_tick;   /* ticks since last offset change */

/* ---- helpers --------------------------------------------------------------- */

static void reset_marquee(void)
{
    s_marquee_offset = 0;
    s_marquee_tick   = 0;
}

static void adjust_scroll(void)
{
    if (s_sel < s_top)                    s_top = s_sel;
    else if (s_sel >= s_top + PB_VISIBLE) s_top = s_sel - PB_VISIBLE + 1;
    reset_marquee();
}

/* Right-truncate with "..." into dst (size must be max_chars+1). */
static void truncate_right(char *dst, size_t dst_sz, const char *src, int max_chars)
{
    int len = (int)strlen(src);
    if (len <= max_chars) {
        snprintf(dst, dst_sz, "%s", src);
    } else {
        snprintf(dst, dst_sz, "%.*s...", max_chars - 3, src);
    }
}

/* Advance the marquee one tick (called once per render from PB_BROWSE). */
#define PB_MARQUEE_PAUSE 2   /* ticks to hold at each end (2 × 250 ms = 500 ms) */

static void marquee_step(int name_len, int max_chars)
{
    int max_off = name_len - max_chars;
    if (max_off <= 0) return;

    s_marquee_tick++;

    bool at_end   = (s_marquee_offset >= max_off);
    bool at_start = (s_marquee_offset == 0);

    if ((at_start || at_end) && s_marquee_tick <= PB_MARQUEE_PAUSE) return; /* hold */

    if (at_end) s_marquee_offset = 0;
    else        s_marquee_offset++;
    s_marquee_tick = 0;
}

/* Which actions are valid for the selected upcoming row (s_sel >= 1). */
static bool action_valid(int action, int len)
{
    switch (action) {
    case PB_ACT_UP:     return s_sel >= 2;
    case PB_ACT_DOWN:   return s_sel < len - 1;
    case PB_ACT_REMOVE: return true;
    default:            return false;
    }
}

/* Default popup selection: Move up when valid, else the first valid action. */
static int default_action(int len)
{
    for (int a = 0; a < PB_N_ACTIONS; a++) {
        if (action_valid(a, len)) return a;
    }
    return PB_ACT_REMOVE;  /* always valid for an upcoming row */
}

/* ---- render: browse -------------------------------------------------------- */

static void render_browse(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);

    int len = (int)playlist_queue_len();
    if (len == 0) {
        gfx_draw_text(PB_TEXT_X, PB_CONTENT_Y + 16, "Queue empty", gfx_rgb(120, 120, 120), 1);
        return;
    }
    if (s_sel >= len) { s_sel = len - 1; adjust_scroll(); }

    int max_name = (GFX_W - PB_TEXT_X) / GFX_CHAR_W; /* (176-4)/6 = 28 */

    for (int i = 0; i < PB_VISIBLE && (s_top + i) < len; i++) {
        int  row = s_top + i;
        int  y   = PB_CONTENT_Y + i * PB_ROW_H;
        bool sel = (row == s_sel);

        playlist_track_t t = {0};
        if (playlist_queue_at((size_t)row, &t) != ESP_OK || !t.name) continue;

        if (sel) gfx_fill_rect(0, y, GFX_W, PB_ROW_H, GFX_WHITE);

        /* row 0 is the current (locked) track: accent it when not selected */
        gfx_color_t fg = sel ? GFX_BLACK
                        : (row == 0 ? gfx_rgb(100, 180, 255) : GFX_WHITE);

        int name_len = (int)strlen(t.name);
        if (sel && name_len > max_name) {
            gfx_draw_text(PB_TEXT_X, y + 2, t.name + s_marquee_offset, fg, 1);
            marquee_step(name_len, max_name);
        } else {
            char label[32];
            truncate_right(label, sizeof label, t.name, max_name);
            gfx_draw_text(PB_TEXT_X, y + 2, label, fg, 1);
        }
    }
}

/* ---- render: action popup -------------------------------------------------- */

#define PB_POPUP_X  20
#define PB_POPUP_Y  50
#define PB_POPUP_W  136
#define PB_POPUP_H  70

static const char *const s_action_labels[PB_N_ACTIONS] = { "Move up", "Move down", "Remove" };

static void render_action(screen_t *self)
{
    render_browse(self);

    int len = (int)playlist_queue_len();

    gfx_fill_rect(PB_POPUP_X - 2, PB_POPUP_Y - 2, PB_POPUP_W + 4, PB_POPUP_H + 4, GFX_BLACK);
    gfx_draw_rect(PB_POPUP_X, PB_POPUP_Y, PB_POPUP_W, PB_POPUP_H, GFX_WHITE);

    playlist_track_t t = {0};
    playlist_queue_at((size_t)s_sel, &t);
    int name_max = (PB_POPUP_W - 8) / GFX_CHAR_W;
    char label[24];
    truncate_right(label, sizeof label, t.name ? t.name : "", name_max);
    gfx_draw_text(PB_POPUP_X + 4, PB_POPUP_Y + 4, label, gfx_rgb(200, 200, 200), 1);
    gfx_fill_rect(PB_POPUP_X + 4, PB_POPUP_Y + 14, PB_POPUP_W - 8, 1, gfx_rgb(70, 70, 70));

    for (int i = 0; i < PB_N_ACTIONS; i++) {
        int  y     = PB_POPUP_Y + 20 + i * 16;
        bool sel   = (i == s_action_sel);
        bool valid = action_valid(i, len);
        if (sel && valid) {
            gfx_fill_rect(PB_POPUP_X + 4, y - 1, PB_POPUP_W - 8, 14, GFX_WHITE);
            gfx_draw_text(PB_POPUP_X + 10, y + 2, s_action_labels[i], GFX_BLACK, 1);
        } else {
            gfx_color_t c = valid ? (sel ? GFX_WHITE : gfx_rgb(200, 200, 200))
                                  : gfx_rgb(80, 80, 80);
            if (sel) gfx_draw_rect(PB_POPUP_X + 4, y - 1, PB_POPUP_W - 8, 14, gfx_rgb(140, 140, 140));
            gfx_draw_text(PB_POPUP_X + 10, y + 2, s_action_labels[i], c, 1);
        }
    }
}

/* ---- input ----------------------------------------------------------------- */

static void do_action(void)
{
    int len = (int)playlist_queue_len();
    if (!action_valid(s_action_sel, len)) return;   /* disabled: no-op, stay in popup */

    esp_err_t err = ESP_FAIL;
    switch (s_action_sel) {
    case PB_ACT_UP:
        err = playlist_queue_move_up((size_t)s_sel);
        if (err == ESP_OK) s_sel--;
        break;
    case PB_ACT_DOWN:
        err = playlist_queue_move_down((size_t)s_sel);
        if (err == ESP_OK) s_sel++;
        break;
    case PB_ACT_REMOVE:
        err = playlist_queue_remove((size_t)s_sel);
        if (err == ESP_OK) {
            int new_len = (int)playlist_queue_len();
            if (s_sel >= new_len) s_sel = new_len - 1;
        }
        break;
    }
    if (err == ESP_OK) {
        adjust_scroll();
        s_state = PB_BROWSE;
    }
}

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    int len = (int)playlist_queue_len();

    switch (s_state) {

    case PB_BROWSE:
        switch (ev) {
        case UI_EVENT_UP:
            if (s_sel > 0) { s_sel--; adjust_scroll(); }
            else           { navigator_pop(); }        /* at the top → back to Now Playing */
            break;
        case UI_EVENT_DOWN:
            if (s_sel < len - 1) { s_sel++; adjust_scroll(); }
            break;
        case UI_EVENT_SELECT:
            if (s_sel >= 1) {                            /* row 0 (current) is locked */
                s_state      = PB_ACTION;
                s_action_sel = default_action(len);      /* Move up when valid */
            }
            break;
        case UI_EVENT_BACK:
            navigator_pop();
            break;
        default: break;
        }
        break;

    case PB_ACTION:
        switch (ev) {
        case UI_EVENT_UP:
            s_action_sel = (s_action_sel - 1 + PB_N_ACTIONS) % PB_N_ACTIONS;
            break;
        case UI_EVENT_DOWN:
            s_action_sel = (s_action_sel + 1) % PB_N_ACTIONS;
            break;
        case UI_EVENT_SELECT:
            do_action();
            break;
        case UI_EVENT_BACK:
            s_state = PB_BROWSE;
            break;
        default: break;
        }
        break;
    }
}

/* ---- vtable ---------------------------------------------------------------- */

static void on_enter(screen_t *self)
{
    (void)self;
    s_state      = PB_BROWSE;
    s_sel        = 0;
    s_top        = 0;
    s_action_sel = PB_ACT_REMOVE;
    reset_marquee();
}

static void pb_on_exit(screen_t *self) { (void)self; }

static void render(screen_t *self)
{
    switch (s_state) {
    case PB_BROWSE: render_browse(self); break;
    case PB_ACTION: render_action(self); break;
    }
}

screen_t *playlist_browser_screen(void)
{
    static screen_t s = {
        .on_enter     = on_enter,
        .on_exit      = pb_on_exit,
        .handle_input = handle_input,
        .render       = render,
        .refresh_ms   = 250,
    };
    return &s;
}
