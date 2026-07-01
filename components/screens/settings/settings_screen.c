/**
 * @file settings_screen.c
 * @brief Settings menu (top-level): a list over three sub-screens, stubs for now.
 *
 * Same shape as the statistics menu: content offset below the status bar, and a sober
 * ">" caret marks the selected row (no full white bar, which on this passive OLED lights
 * OFF pixels across the line; see root_menu's note). The sub-screen pointers aren't
 * compile-time constants, so they are seeded on first use.
 */
#include "settings_screen.h"
#include "bluetooth_settings.h"
#include "audio_settings.h"
#include "screen_settings.h"
#include "navigator.h"
#include "status_bar.h"
#include "gfx.h"

#define N_ITEMS      3
#define MENU_ROW_H   28
#define MENU_Y0      (STATUS_BAR_H + 6)
#define MENU_ARROW_X 4
#define MENU_TEXT_X  22
#define MENU_SCALE   2

static const char *const s_labels[N_ITEMS] = { "Bluetooth", "Audio", "Screen" };
static screen_t         *s_targets[N_ITEMS];   /* seeded on first getter call */
static int s_sel = 0;

static void noop(screen_t *self) { (void)self; }

static void menu_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    switch (ev) {
    case UI_EVENT_UP:   if (s_sel > 0)            s_sel--; break;
    case UI_EVENT_DOWN: if (s_sel < N_ITEMS - 1)  s_sel++; break;
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
            gfx_draw_text(MENU_ARROW_X, y, ">", gfx_rgb(255, 0, 0), MENU_SCALE);   /* Settings = red */
        gfx_draw_text(MENU_TEXT_X, y, s_labels[i], GFX_WHITE, MENU_SCALE);
    }
}

static screen_t s_menu = {
    .on_enter     = noop,
    .on_exit      = noop,
    .handle_input = menu_input,
    .render       = menu_render,
};

screen_t *settings_screen(void)
{
    static int done = 0;
    if (!done) {
        s_targets[0] = bluetooth_settings_screen();
        s_targets[1] = audio_settings_screen();
        s_targets[2] = screen_settings_screen();
        done = 1;
    }
    return &s_menu;
}
