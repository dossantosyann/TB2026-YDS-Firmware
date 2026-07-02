/**
 * @file screen_settings.c
 * @brief Screen (display) settings: a single brightness slider.
 *
 * LEFT/RIGHT dim/brighten the panel; the change is applied live to the SSD1333 Master
 * Current for immediate feedback, and persisted to NVS ("oled_bri") once on exit -- only
 * if it actually moved, so a still slider costs no flash write. Restored at boot by
 * screen_settings_restore(), called from app_run once the panel is up.
 *
 * The bottom of the range is floored at BRI_MIN (never 0): a fully dimmed panel would be
 * unreadable, leaving the user unable to see the slider to raise it back.
 */
#include "screen_settings.h"
#include "navigator.h"
#include "status_bar.h"
#include "gfx.h"
#include "settings.h"
#include "display_oled.h"

#include <stdio.h>

#define NVS_KEY   "oled_bri"
#define BRI_MIN   2                              /* keep the UI legible at the low end */
#define BRI_MAX   DISPLAY_OLED_BRIGHTNESS_MAX    /* 15 */
#define BRI_DFLT  BRI_MAX                         /* first boot: full brightness (init default) */

#define ACCENT    gfx_rgb(255, 0, 0)             /* Settings identity colour (see root_menu) */
#define DIM       gfx_rgb(110, 110, 110)

/* Single control, vertically centred in the content area below the status bar. */
#define CONTENT_TOP (STATUS_BAR_H)
#define MID_Y       ((CONTENT_TOP + GFX_H) / 2)
#define HDR_Y       (MID_Y - 22)
#define BAR_X       18
#define BAR_W       140
#define BAR_Y       (MID_Y + 2)
#define BAR_H       6
#define VAL_Y       (BAR_Y + 18)

static uint8_t s_level;    /* live brightness, BRI_MIN..BRI_MAX */
static uint8_t s_saved;    /* value on entry, to detect a real change on exit */

static void render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);

    gfx_draw_text(4, HDR_Y, ">", ACCENT, 1);
    gfx_draw_text(16, HDR_Y, "BRIGHTNESS", DIM, 1);

    /* Track with the filled portion up to the knob. */
    int span = BRI_MAX - BRI_MIN;
    int fill = (s_level - BRI_MIN) * BAR_W / span;
    gfx_fill_rect(BAR_X, BAR_Y + BAR_H / 2 - 1, BAR_W, 2, DIM);
    gfx_fill_rect(BAR_X, BAR_Y + BAR_H / 2 - 1, fill, 2, ACCENT);
    gfx_fill_rect(BAR_X + fill - 2, BAR_Y - 3, 5, BAR_H + 6, GFX_WHITE);

    char buf[8];
    snprintf(buf, sizeof buf, "%d%%", s_level * 100 / BRI_MAX);
    gfx_draw_text(BAR_X, VAL_Y, buf, GFX_WHITE, 1);
}

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    switch (ev) {
    case UI_EVENT_RIGHT:
        if (s_level < BRI_MAX) display_oled_set_brightness(++s_level);
        break;
    case UI_EVENT_LEFT:
        if (s_level > BRI_MIN) display_oled_set_brightness(--s_level);
        break;
    case UI_EVENT_BACK:
        navigator_pop();
        break;
    default:
        break;
    }
}

static void screen_on_enter(screen_t *self) { (void)self; s_saved = s_level; }

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
