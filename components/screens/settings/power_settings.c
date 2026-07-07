/**
 * @file power_settings.c
 * @brief Power sub-menu: sleep delay, auto power-off delay, and manual power-off.
 *
 * A three-row list (same look as the top Settings menu). The first two rows open
 * duration_picker screens bound to the two user-adjustable idle timeouts in the power
 * service (screen sleep, auto power-off). The third opens the power-off confirmation:
 * SELECT releases EnableReg and cuts the rail (power_shutdown() never returns), BACK
 * cancels. The rail is also latched on by USB, so shutting down while USB is connected
 * would leave the board up while power_shutdown() spins forever -- so we gate on the
 * cached external-power flag and, when USB is present, refuse and tell the user to
 * unplug instead of walking into that dead end.
 */
#include "power_settings.h"
#include "duration_picker.h"
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

/* Preset values in ms; the trailing 0 renders as "Never" (policy disabled). Each list
   includes the service default so the highlight seeds onto a real row on first entry. */
static const uint32_t k_sleep_presets[] = { 15000, 30000, 60000, 120000, 300000, 0 };
static const uint32_t k_off_presets[]   = { 60000, 120000, 300000, 600000, 1800000, 0 };

static duration_picker_t s_sleep_picker;
static duration_picker_t s_off_picker;

/* ---- Power-off confirmation screen ------------------------------------------------ */

static void draw_centred(int y, const char *s, gfx_color_t c, int scale)
{
    int w = (int)strlen(s) * GFX_CHAR_W * scale;
    gfx_draw_text((GFX_W - w) / 2, y, s, c, scale);
}

static void confirm_render(screen_t *self)
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

static void confirm_input(screen_t *self, ui_event_t ev)
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

static screen_t s_confirm = {
    .on_enter     = noop,
    .on_exit      = noop,
    .handle_input = confirm_input,
    .render       = confirm_render,
};

/* ---- Power sub-menu ---------------------------------------------------------------- */

#define N_ITEMS      3
#define MENU_ROW_H   40
#define MENU_Y0      (STATUS_BAR_H + 12)
#define MENU_ARROW_X 2
#define MENU_TEXT_X  22
#define MENU_SCALE   2

static const char *const s_labels[N_ITEMS] = { "Sleep", "Auto off", "Power off" };
static screen_t         *s_targets[N_ITEMS];   /* seeded on first getter call */
static int s_sel = 0;

static void menu_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    switch (ev) {
    case UI_EVENT_UP:   s_sel = (s_sel - 1 + N_ITEMS) % N_ITEMS; break;
    case UI_EVENT_DOWN: s_sel = (s_sel + 1) % N_ITEMS;          break;
    case UI_EVENT_SELECT: navigator_push(s_targets[s_sel]); break;
    case UI_EVENT_BACK:   navigator_pop();                  break;
    default: break;
    }
}

static void menu_render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);
    for (int i = 0; i < N_ITEMS; i++) {
        int y = MENU_Y0 + i * MENU_ROW_H;
        if (i == s_sel)
            gfx_draw_text(MENU_ARROW_X, y, ">", ACCENT, MENU_SCALE);
        gfx_draw_text(MENU_TEXT_X, y, s_labels[i], GFX_WHITE, MENU_SCALE);
    }
}

static screen_t s_menu = {
    .on_enter     = noop,
    .on_exit      = noop,
    .handle_input = menu_input,
    .render       = menu_render,
};

screen_t *power_settings_screen(void)
{
    static int done = 0;
    if (!done) {
        duration_picker_init(&s_sleep_picker, "SLEEP DELAY",
                             k_sleep_presets, sizeof k_sleep_presets / sizeof k_sleep_presets[0],
                             power_get_sleep_ms, power_set_sleep_ms);
        duration_picker_init(&s_off_picker, "AUTO POWER OFF",
                             k_off_presets, sizeof k_off_presets / sizeof k_off_presets[0],
                             power_get_poweroff_ms, power_set_poweroff_ms);
        s_targets[0] = &s_sleep_picker.base;
        s_targets[1] = &s_off_picker.base;
        s_targets[2] = &s_confirm;
        done = 1;
    }
    return &s_menu;
}
