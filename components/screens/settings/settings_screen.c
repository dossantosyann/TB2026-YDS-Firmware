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
#include "power_settings.h"
#include "navigator.h"
#include "status_bar.h"
#include "gfx.h"
#include "icons.h"

#define N_ITEMS      4
#define MENU_ROW_H   40                 /* 160px content / 4 items = 40px even slices */
#define MENU_Y0      (STATUS_BAR_H + 12) /* symmetric top/bottom margins */
#define MENU_ARROW_X 2
#define MENU_ICON_X  18
#define MENU_TEXT_X  40
#define MENU_SCALE   2

static const char *const s_labels[N_ITEMS] = { "Bluetooth", "Audio", "Screen", "Power off" };
static screen_t         *s_targets[N_ITEMS];   /* seeded on first getter call */
/* 16x16 type icons, parallel to s_labels; NULL until the icon exists. */
static const uint8_t *const s_icons[N_ITEMS] = { NULL, NULL, NULL, NULL };
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
    gfx_color_t accent = gfx_rgb(255, 0, 0);   /* Settings = red */
    for (int i = 0; i < N_ITEMS; i++) {
        int y = MENU_Y0 + i * MENU_ROW_H;
        bool sel = (i == s_sel);
        if (sel)
            gfx_draw_text(MENU_ARROW_X, y, ">", accent, MENU_SCALE);
        if (s_icons[i])
            gfx_blit_1bpp(MENU_ICON_X, y - 1, 16, 16,
                          s_icons[i], sel ? accent : GFX_WHITE);   /* -1: center 16px icon on 14px text */
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
        s_targets[3] = power_settings_screen();
        done = 1;
    }
    return &s_menu;
}
