#include "root_menu.h"
#include "music_screen.h"
#include "storage_screen.h"
#include "stats_screen.h"
#include "settings_screen.h"
#include "navigator.h"
#include "gfx.h"
#include "icons.h"

/* Home layout: a vertical list of 4 entries (Music, Storage, Stats, Settings) on
   a black background. Each row holds a 32x32 icon on the left and a scale-2 text
   label to its right. The selected row is marked by 4 short white corner brackets
   spanning the full row width -- never a full box or a solid horizontal bar, which
   on this passive OLED would light up OFF pixels across the whole line (IR drop on
   the shared common). Unselected rows carry no decoration; the black background and
   interiors keep the count of lit pixels (and so power draw) minimal. */
#define ROW_H      44                        /* 4 rows * 44 = 176: fills the panel exactly */
#define N_ROWS     4
#define ICON_SZ    32                        /* all menu icons are 32x32 (see icons.h) */
#define ICON_X     8
#define ICON_INSET ((ROW_H - ICON_SZ) / 2)   /* 6px: vertically center the icon in the row */
#define TEXT_X     48
#define TEXT_SCALE 2
#define BORDER     2                         /* bracket thickness in pixels */
#define BRACKET    8                         /* bracket arm length in pixels */

/* One icon per row, top to bottom. All menu icons are 32x32 (see icons.h). */
static const uint8_t *const s_icons[N_ROWS] = {
    icon_music,
    icon_storage,
    icon_stats,
    icon_settings,
};

/* Label drawn to the right of each icon, same order as s_icons. */
static const char *const s_labels[N_ROWS] = {
    "Music",
    "Storage",
    "Stats",
    "Settings",
};

/* Destination screen per row (same order as s_icons). Stubs for now, so the whole
   button navigation can be exercised; each gets fleshed out later. */
static screen_t *s_targets[N_ROWS];

/* Accent color drawn on a row's icon while that row is selected; unselected icons
   stay white. Seeded at startup since gfx_rgb() isn't a constant expression. Coloring
   only the selected icon also keeps the count of lit pixels (and power) down. */
static gfx_color_t s_accent[N_ROWS];

static int s_selected = 0;   /* highlighted row; file-static singleton state */

/* Seed the once-only mutable state: target screens and accent colors. Called from
   root_menu() at startup, before any input reaches handle_input(). */
static void ensure_targets(void)
{
    static int done = 0;
    if (done) return;
    s_targets[0] = music_screen();
    s_targets[1] = storage_screen();
    s_targets[2] = stats_screen();
    s_targets[3] = settings_screen();
    s_accent[0] = gfx_rgb(0, 100, 255);   /* Music    - blue       */
    s_accent[1] = gfx_rgb(0, 255, 0);     /* Storage  - green      */
    s_accent[2] = gfx_rgb(255, 120, 0);   /* Stats    - orange     */
    s_accent[3] = gfx_rgb(255, 0, 0);     /* Settings - red        */
    done = 1;
}

static void on_enter(screen_t *self) { (void)self; }
static void on_exit(screen_t *self)  { (void)self; }

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    switch (ev) {
    case UI_EVENT_UP:   if (s_selected > 0)          s_selected--; break;
    case UI_EVENT_DOWN: if (s_selected < N_ROWS - 1) s_selected++; break;
    case UI_EVENT_SELECT:
        if (s_targets[s_selected]) navigator_push(s_targets[s_selected]);
        break;
    default: break;   /* BACK is a no-op: this is the home screen. */
    }
}

/* Draw the 4 white corner brackets around a full-width row at top y. Short arms
   only -- no continuous horizontal run -- to dodge the passive-OLED row halo. */
static void draw_brackets(int y)
{
    int b = y + ROW_H;          /* one past the bottom edge */
    int r = GFX_W;              /* one past the right edge  */
    gfx_fill_rect(0,           y,           BRACKET, BORDER,  GFX_WHITE);  /* top-left  */
    gfx_fill_rect(0,           y,           BORDER,  BRACKET, GFX_WHITE);
    gfx_fill_rect(r - BRACKET, y,           BRACKET, BORDER,  GFX_WHITE);  /* top-right */
    gfx_fill_rect(r - BORDER,  y,           BORDER,  BRACKET, GFX_WHITE);
    gfx_fill_rect(0,           b - BORDER,  BRACKET, BORDER,  GFX_WHITE);  /* bot-left  */
    gfx_fill_rect(0,           b - BRACKET, BORDER,  BRACKET, GFX_WHITE);
    gfx_fill_rect(r - BRACKET, b - BORDER,  BRACKET, BORDER,  GFX_WHITE);  /* bot-right */
    gfx_fill_rect(r - BORDER,  b - BRACKET, BORDER,  BRACKET, GFX_WHITE);
}

static void render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);

    for (int i = 0; i < N_ROWS; i++) {
        int y = i * ROW_H;
        gfx_color_t icon_color = (i == s_selected) ? s_accent[i] : GFX_WHITE;
        if (i == s_selected) draw_brackets(y);
        gfx_blit_1bpp(ICON_X, y + ICON_INSET, ICON_SZ, ICON_SZ, s_icons[i], icon_color);
        int ty = y + (ROW_H - GFX_CHAR_H * TEXT_SCALE) / 2;  /* vertically center the label */
        gfx_draw_text(TEXT_X, ty, s_labels[i], GFX_WHITE, TEXT_SCALE);
    }
}

screen_t *root_menu(void)
{
    static screen_t s = {
        .on_enter     = on_enter,
        .on_exit      = on_exit,
        .handle_input = handle_input,
        .render       = render,
    };
    ensure_targets();
    return &s;
}
