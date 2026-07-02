/**
 * @file power_settings.c
 * @brief Power-off confirmation screen: on confirm, release EnableReg to cut the rail.
 *
 * SELECT powers the board off (power_shutdown() drives PIN_REG_EN low and never returns);
 * BACK cancels. The rail is also latched on by USB, so shutting down while USB is
 * connected would leave the board up while power_shutdown() spins forever -- so we gate
 * on the cached external-power flag and, when USB is present, refuse and tell the user to
 * unplug instead of walking into that dead end.
 */
#include "power_settings.h"
#include "navigator.h"
#include "status_bar.h"
#include "gfx.h"
#include "power.h"

#include <string.h>

#define ACCENT gfx_rgb(255, 0, 0)             /* Settings identity colour (see root_menu) */
#define DIM    gfx_rgb(110, 110, 110)
#define WARN   gfx_rgb(255, 180, 0)

#define CONTENT_TOP (STATUS_BAR_H)
#define MID_Y       ((CONTENT_TOP + GFX_H) / 2)

static void draw_centred(int y, const char *s, gfx_color_t c, int scale)
{
    int w = (int)strlen(s) * GFX_CHAR_W * scale;
    gfx_draw_text((GFX_W - w) / 2, y, s, c, scale);
}

static void render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);

    power_state_t st;
    power_get_state(&st);

    draw_centred(MID_Y - 34, "POWER OFF", ACCENT, 2);

    if (st.external_power) {
        draw_centred(MID_Y + 2,  "USB connected", WARN, 1);
        draw_centred(MID_Y + 16, "Unplug to power off", DIM, 1);
    } else {
        draw_centred(MID_Y + 2,  "SELECT = power off", GFX_WHITE, 1);
        draw_centred(MID_Y + 16, "BACK = cancel", DIM, 1);
    }
}

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    switch (ev) {
    case UI_EVENT_SELECT: {
        power_state_t st;
        power_get_state(&st);
        if (!st.external_power) power_shutdown();   /* never returns */
        /* USB latches the rail on: refuse, render() shows the warning. */
        break;
    }
    case UI_EVENT_BACK:
        navigator_pop();
        break;
    default:
        break;
    }
}

static void noop(screen_t *self) { (void)self; }

screen_t *power_settings_screen(void)
{
    static screen_t s = {
        .on_enter     = noop,
        .on_exit      = noop,
        .handle_input = handle_input,
        .render       = render,
    };
    return &s;
}
