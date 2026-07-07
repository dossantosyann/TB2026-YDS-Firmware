/**
 * @file screen_settings.c
 * @brief Screen (display) settings: brightness slider + auto-sleep delay.
 *
 * Two vertically stacked zones; UP/DOWN moves the focus between them:
 *   - Brightness: LEFT/RIGHT dim/brighten the panel. The change is applied live to the
 *                 SSD1333 Master Current for immediate feedback, and persisted to NVS
 *                 ("oled_bri") once on exit -- only if it actually moved. Restored at boot
 *                 by screen_settings_restore(), called from app_run once the panel is up.
 *   - Sleep     : horizontal preset picker (LEFT/RIGHT cycles, wrapping) for the screen
 *                 auto-sleep delay. This is the same value edited under Settings > Power;
 *                 it is duplicated here (Screen is the natural home for a display timeout)
 *                 and stays in sync because both screens read/write the power service.
 *
 * The bottom of the brightness range is floored at BRI_MIN (never 0): a fully dimmed panel
 * would be unreadable, leaving the user unable to see the slider to raise it back. The
 * sleep delay is persisted through the power service on BACK, only if it actually moved
 * (Rule 13: no NVS write per scroll step).
 */
#include "screen_settings.h"
#include "navigator.h"
#include "status_bar.h"
#include "gfx.h"
#include "settings.h"
#include "display_oled.h"
#include "power.h"

#include <stdio.h>
#include <string.h>

#define NVS_KEY   "oled_bri"
#define BRI_MIN   2                              /* keep the UI legible at the low end */
#define BRI_MAX   DISPLAY_OLED_BRIGHTNESS_MAX    /* 15 */
#define BRI_DFLT  BRI_MAX                         /* first boot: full brightness (init default) */

#define ACCENT    gfx_rgb(255, 0, 0)             /* Settings identity colour (see root_menu) */
#define DIM       gfx_rgb(110, 110, 110)

/* Screen-sleep presets in ms; the trailing 0 renders as "Never" (policy disabled). Kept in
   step with the Sleep list in power_settings.c -- both edit the same service value. */
static const uint32_t k_sleep_presets[] = { 15000, 30000, 60000, 120000, 300000, 0 };
#define N_SLEEP (int)(sizeof k_sleep_presets / sizeof k_sleep_presets[0])

/* Two focusable zones, top to bottom. */
enum { ZONE_BRI = 0, ZONE_SLEEP, N_ZONES };

/* Brightness slider, upper half of the content area below the status bar. */
#define BRI_HDR_Y   44
#define BAR_X       18
#define BAR_W       140
#define BAR_Y       66
#define BAR_H       6
#define VAL_Y       (BAR_Y + 18)

/* Sleep picker, lower half. */
#define SLEEP_TITLE_Y 116
#define SLEEP_VAL_Y   134

static uint8_t s_level;      /* live brightness, BRI_MIN..BRI_MAX */
static uint8_t s_saved;      /* value on entry, to detect a real change on exit */
static int     s_focus;      /* which zone owns LEFT/RIGHT */
static int     s_sleep_sel;  /* index into k_sleep_presets */

/* Human-readable duration into buf: "Never" (0), "30 s" (<1 min), else whole minutes. */
static void fmt_duration(uint32_t ms, char *buf, size_t n)
{
    if (ms == 0)             snprintf(buf, n, "Never");
    else if (ms < 60000u)    snprintf(buf, n, "%u s", (unsigned)(ms / 1000u));
    else                     snprintf(buf, n, "%u min", (unsigned)(ms / 60000u));
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

static void render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);

    bool bri_focus = (s_focus == ZONE_BRI);
    gfx_draw_text(4, BRI_HDR_Y, ">", bri_focus ? ACCENT : DIM, 1);
    gfx_draw_text(16, BRI_HDR_Y, "BRIGHTNESS", bri_focus ? ACCENT : DIM, 1);

    /* Track with the filled portion up to the knob. */
    int span = BRI_MAX - BRI_MIN;
    int fill = (s_level - BRI_MIN) * BAR_W / span;
    gfx_fill_rect(BAR_X, BAR_Y + BAR_H / 2 - 1, BAR_W, 2, DIM);
    gfx_fill_rect(BAR_X, BAR_Y + BAR_H / 2 - 1, fill, 2, bri_focus ? ACCENT : DIM);
    gfx_fill_rect(BAR_X + fill - 2, BAR_Y - 3, 5, BAR_H + 6, bri_focus ? GFX_WHITE : DIM);

    char buf[12];
    snprintf(buf, sizeof buf, "%d%%", s_level * 100 / BRI_MAX);
    gfx_draw_text(BAR_X, VAL_Y, buf, bri_focus ? GFX_WHITE : DIM, 1);

    /* Sleep picker: title, current value centred at scale 2, framed by "<" ">" arrows
       when focused (the cue that LEFT/RIGHT cycles it). */
    bool sleep_focus = (s_focus == ZONE_SLEEP);
    gfx_draw_text(8, SLEEP_TITLE_Y, "Sleep", sleep_focus ? ACCENT : DIM, 1);
    fmt_duration(k_sleep_presets[s_sleep_sel], buf, sizeof buf);
    draw_centred(SLEEP_VAL_Y, buf, sleep_focus ? GFX_WHITE : DIM, 2);
    if (sleep_focus) {
        gfx_draw_text(4, SLEEP_VAL_Y, "<", ACCENT, 2);
        gfx_draw_text(GFX_W - 4 - GFX_CHAR_W * 2, SLEEP_VAL_Y, ">", ACCENT, 2);
    }
}

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    switch (ev) {
    case UI_EVENT_UP:   s_focus = (s_focus - 1 + N_ZONES) % N_ZONES; break;
    case UI_EVENT_DOWN: s_focus = (s_focus + 1) % N_ZONES;           break;
    case UI_EVENT_RIGHT:
        if (s_focus == ZONE_BRI) {
            if (s_level < BRI_MAX) display_oled_set_brightness(++s_level);
        } else {
            s_sleep_sel = (s_sleep_sel + 1) % N_SLEEP;
        }
        break;
    case UI_EVENT_LEFT:
        if (s_focus == ZONE_BRI) {
            if (s_level > BRI_MIN) display_oled_set_brightness(--s_level);
        } else {
            s_sleep_sel = (s_sleep_sel - 1 + N_SLEEP) % N_SLEEP;
        }
        break;
    case UI_EVENT_BACK:
        /* Persist the sleep delay once on the way out, only if it moved. Brightness is
           persisted separately in screen_on_exit (called by navigator_pop). */
        if (k_sleep_presets[s_sleep_sel] != power_get_sleep_ms())
            power_set_sleep_ms(k_sleep_presets[s_sleep_sel]);
        navigator_pop();
        break;
    default:
        break;
    }
}

/* Seed the highlight from the stored value; fall back to the first row if it isn't a
   preset (should not happen -- every persisted value comes from this list). */
static int find_index(uint32_t val)
{
    for (int i = 0; i < N_SLEEP; i++)
        if (k_sleep_presets[i] == val) return i;
    return 0;
}

static void screen_on_enter(screen_t *self)
{
    (void)self;
    s_saved     = s_level;
    s_focus     = ZONE_BRI;
    s_sleep_sel = find_index(power_get_sleep_ms());
}

static void screen_on_exit(screen_t *self)
{
    (void)self;
    if (s_level != s_saved) settings_set_u8(NVS_KEY, s_level);   /* persist once, only if moved */
}

void screen_settings_restore(void)
{
    uint8_t v;
    if (settings_get_u8(NVS_KEY, &v) != ESP_OK || v < BRI_MIN || v > BRI_MAX)
        v = BRI_DFLT;                            /* unset or out of range: full brightness */
    s_level = v;
    display_oled_set_brightness(s_level);
}

screen_t *screen_settings_screen(void)
{
    static screen_t s = {
        .on_enter     = screen_on_enter,
        .on_exit      = screen_on_exit,
        .handle_input = handle_input,
        .render       = render,
    };
    return &s;
}
