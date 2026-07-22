/**
 * @file power_settings.c
 * @brief Power sub-menu on a single screen: sleep delay, auto power-off, and power off.
 *
 * One screen split into three vertical zones; UP/DOWN moves the focus between them:
 *   - Sleep    : horizontal preset picker (LEFT/RIGHT cycles, wrapping) for the screen
 *                sleep delay.
 *   - Auto off : same, for the auto power-off delay.
 *   - Power off: a centred button. SELECT opens a confirmation popup with Confirm/Cancel.
 *
 * The two idle timeouts are persisted through the power service on BACK, only if they
 * actually moved (Rule 13: no NVS write per scroll step).
 *
 * Power-off is gated on external power: SELECT on Confirm releases EnableReg and cuts the
 * rail (power_shutdown() never returns). The rail is also latched on by USB, so shutting
 * down while USB is connected would spin power_shutdown() forever -- when USB is present
 * the popup shows a warning and refuses instead of walking into that dead end.
 */
#include "power_settings.h"
#include "navigator.h"
#include "status_bar.h"
#include "gfx.h"
#include "power.h"
#include "player.h"

#include <stdio.h>
#include <string.h>

#define ACCENT gfx_rgb(255, 0, 0)             /* Settings identity colour (see root_menu) */
#define DIM    gfx_rgb(110, 110, 110)
#define WARN   gfx_rgb(255, 180, 0)

/* Preset values in ms; the trailing 0 renders as "Never" (policy disabled). Each list
   includes the service default so the highlight seeds onto a real row on first entry. */
static const uint32_t k_sleep_presets[] = { 15000, 30000, 60000, 120000, 300000, 0 };
static const uint32_t k_off_presets[]   = { 60000, 120000, 300000, 600000, 1800000, 0 };
#define N_SLEEP (int)(sizeof k_sleep_presets / sizeof k_sleep_presets[0])
#define N_OFF   (int)(sizeof k_off_presets   / sizeof k_off_presets[0])

/* Three focusable zones, top to bottom. */
enum { ZONE_SLEEP = 0, ZONE_OFF, ZONE_POWEROFF, N_ZONES };

static int  s_focus;        /* which zone owns LEFT/RIGHT and SELECT */
static int  s_sleep_sel;    /* index into k_sleep_presets */
static int  s_off_sel;      /* index into k_off_presets */
static bool s_popup;        /* power-off confirmation overlay is up */
static int  s_confirm_sel;  /* 0 = Confirm, 1 = Cancel (defaults to Cancel: destructive) */

/* Layout (below the 16px status bar). */
#define SLEEP_TITLE_Y 26
#define SLEEP_VAL_Y   44
#define OFF_TITLE_Y   84
#define OFF_VAL_Y     102
#define BTN_Y         140
#define BTN_H         26

/* Human-readable duration into buf: "Never" (0), "30 s" (<1 min), else whole minutes. */
static void fmt_duration(uint32_t ms, char *buf, size_t n)
{
    if (ms == 0)             snprintf(buf, n, "Never");
    else if (ms < 60000u)    snprintf(buf, n, "%u s", (unsigned)(ms / 1000u));
    else                     snprintf(buf, n, "%u min", (unsigned)(ms / 60000u));
}

static int find_index(const uint32_t *presets, int count, uint32_t val)
{
    for (int i = 0; i < count; i++)
        if (presets[i] == val) return i;
    return 0;
}

/* Rendered glyph run width, excluding the trailing 1px inter-char gap the font cell adds
   after the last glyph -- without this the centring is off by one scaled pixel. */
static int text_w(const char *s, int scale)
{
    int n = (int)strlen(s);
    return n ? n * GFX_CHAR_W * scale - scale : 0;
}

static void draw_centred(int y, const char *s, gfx_color_t c, int scale)
{
    gfx_draw_text((GFX_W - text_w(s, scale)) / 2, y, s, c, scale);
}

/* Centre a string horizontally inside the box [x0, x0+box_w). */
static void draw_centred_in(int x0, int box_w, int y, const char *s, gfx_color_t c, int scale)
{
    gfx_draw_text(x0 + (box_w - text_w(s, scale)) / 2, y, s, c, scale);
}

/* One horizontal preset zone: title, then the current value centred at scale 2, framed by
   "<" ">" arrows when this zone has focus (the cue that LEFT/RIGHT cycles it). */
static void draw_picker(int title_y, int val_y, const char *title,
                        uint32_t value, bool focused)
{
    gfx_draw_text(8, title_y, title, focused ? ACCENT : DIM, 1);

    char buf[12];
    fmt_duration(value, buf, sizeof buf);
    draw_centred(val_y, buf, focused ? GFX_WHITE : DIM, 2);

    if (focused) {
        gfx_draw_text(4, val_y, "<", ACCENT, 2);
        gfx_draw_text(GFX_W - 4 - GFX_CHAR_W * 2, val_y, ">", ACCENT, 2);
    }
}

static void draw_power_button(bool focused)
{
    const char *label = "POWER OFF";
    int scale = 2;
    int bw = text_w(label, scale) + 20;
    int bx = (GFX_W - bw) / 2;
    /* Glyph run height is GFX_CHAR_H*scale minus the cell's trailing gap (scale px). */
    int ty = BTN_Y + (BTN_H - (GFX_CHAR_H * scale - scale)) / 2;
    gfx_color_t c = focused ? ACCENT : DIM;
    gfx_draw_rect(bx, BTN_Y, bw, BTN_H, c);
    draw_centred_in(bx, bw, ty, label, focused ? GFX_WHITE : DIM, scale);
}

/* ---- confirmation popup ------------------------------------------------------------ */

static void render_popup(void)
{
    power_state_t st;
    power_get_state(&st);

    const int bw = 150, bh = 64;
    const int bx = (GFX_W - bw) / 2, by = (GFX_H - bh) / 2;
    gfx_fill_rect(bx, by, bw, bh, GFX_BLACK);
    gfx_draw_rect(bx, by, bw, bh, GFX_WHITE);

    if (st.external_power) {
        gfx_draw_text(bx + 6, by + 8,  "USB connected",       WARN, 1);
        gfx_draw_text(bx + 6, by + 24, "Unplug to power off", DIM,  1);
        gfx_draw_text(bx + 6, by + 46, "BACK to close",       DIM,  1);
        return;
    }

    draw_centred_in(bx, bw, by + 10, "Power off?", ACCENT, 1);

    int confirm_x = bx + 16, cancel_x = bx + 86, oy = by + 42;
    gfx_draw_text((s_confirm_sel == 0 ? confirm_x : cancel_x) - 8, oy, ">", ACCENT, 1);
    gfx_draw_text(confirm_x, oy, "Confirm", GFX_WHITE, 1);
    gfx_draw_text(cancel_x,  oy, "Cancel",  GFX_WHITE, 1);
}

static void popup_input(ui_event_t ev)
{
    power_state_t st;
    power_get_state(&st);

    if (st.external_power) {                 /* only way out is to dismiss */
        if (ev == UI_EVENT_BACK || ev == UI_EVENT_SELECT) s_popup = false;
        return;
    }

    switch (ev) {
    case UI_EVENT_UP:
    case UI_EVENT_DOWN:
    case UI_EVENT_LEFT:
    case UI_EVENT_RIGHT:  s_confirm_sel ^= 1; break;
    case UI_EVENT_SELECT:
        if (s_confirm_sel == 0) {
            player_save_resume();        /* re-select this track in Now Playing at the next boot */
            power_shutdown();            /* never returns */
        }
        s_popup = false;
        break;
    case UI_EVENT_BACK:   s_popup = false; break;
    default: break;
    }
}

/* ---- screen ------------------------------------------------------------------------ */

static void on_enter(screen_t *self)
{
    (void)self;
    s_sleep_sel = find_index(k_sleep_presets, N_SLEEP, power_get_sleep_ms());
    s_off_sel   = find_index(k_off_presets,   N_OFF,   power_get_poweroff_ms());
    s_focus     = ZONE_SLEEP;
    s_popup     = false;
}

static void render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);

    draw_picker(SLEEP_TITLE_Y, SLEEP_VAL_Y, "Sleep",
                k_sleep_presets[s_sleep_sel], s_focus == ZONE_SLEEP);
    draw_picker(OFF_TITLE_Y, OFF_VAL_Y, "Auto off",
                k_off_presets[s_off_sel], s_focus == ZONE_OFF);
    draw_power_button(s_focus == ZONE_POWEROFF);

    if (s_popup) render_popup();
}

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;

    if (s_popup) { popup_input(ev); return; }

    switch (ev) {
    case UI_EVENT_UP:   s_focus = (s_focus - 1 + N_ZONES) % N_ZONES; break;
    case UI_EVENT_DOWN: s_focus = (s_focus + 1) % N_ZONES;           break;
    case UI_EVENT_LEFT:
        if (s_focus == ZONE_SLEEP)    s_sleep_sel = (s_sleep_sel - 1 + N_SLEEP) % N_SLEEP;
        else if (s_focus == ZONE_OFF) s_off_sel   = (s_off_sel   - 1 + N_OFF)   % N_OFF;
        break;
    case UI_EVENT_RIGHT:
        if (s_focus == ZONE_SLEEP)    s_sleep_sel = (s_sleep_sel + 1) % N_SLEEP;
        else if (s_focus == ZONE_OFF) s_off_sel   = (s_off_sel   + 1) % N_OFF;
        break;
    case UI_EVENT_SELECT:
        if (s_focus == ZONE_POWEROFF) { s_confirm_sel = 1; s_popup = true; }
        break;
    case UI_EVENT_BACK:
        /* Persist once on the way out, only the timeouts that actually moved. */
        if (k_sleep_presets[s_sleep_sel] != power_get_sleep_ms())
            power_set_sleep_ms(k_sleep_presets[s_sleep_sel]);
        if (k_off_presets[s_off_sel] != power_get_poweroff_ms())
            power_set_poweroff_ms(k_off_presets[s_off_sel]);
        navigator_pop();
        break;
    default: break;
    }
}

static void noop(screen_t *self) { (void)self; }

static screen_t s_screen = {
    .on_enter     = on_enter,
    .on_exit      = noop,
    .handle_input = handle_input,
    .render       = render,
};

screen_t *power_settings_screen(void)
{
    return &s_screen;
}
