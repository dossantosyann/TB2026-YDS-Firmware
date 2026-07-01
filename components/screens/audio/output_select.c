/**
 * @file output_select.c
 * @brief Output-selection popup: route audio to the Jack or a Bluetooth speaker.
 *
 * A small centered box (drawn over black) reached from Now Playing (UP). Three rows: the wired
 * Jack, a Bluetooth speaker, and a shortcut into the Bluetooth settings screen. The Bluetooth
 * row is dimmed and inert while no device is connected. Selecting an output calls the single
 * routing entry point player_set_output(), which re-routes a live stream and powers the unused
 * path down itself -- so this screen only expresses the choice, it does not touch hardware.
 *
 * A Bluetooth switch can be refused when the speaker has not yet acknowledged its volume
 * (never blast): in that case the popup stays open and nothing changes.
 */
#include "output_select.h"
#include "bluetooth_settings.h"
#include "navigator.h"
#include "gfx.h"
#include "player.h"
#include "volume.h"
#include "bluetooth.h"

/* ---- layout: a centered box over black ------------------------------------------- */
#define BOX_W    148
#define BOX_H    84
#define BOX_X    ((GFX_W - BOX_W) / 2)
#define BOX_Y    ((GFX_H - BOX_H) / 2)

#define TITLE_Y  (BOX_Y + 6)
#define ROW0_Y   (BOX_Y + 26)
#define ROW_H    20
#define CARET_X  (BOX_X + 8)
#define LABEL_X  (BOX_X + 20)
#define DOT_X    (BOX_X + BOX_W - 14)   /* active-output marker, right-aligned */

#define ACCENT   gfx_rgb(0, 100, 255)   /* Music identity colour (see root_menu) */
#define DIM      gfx_rgb(110, 110, 110)

/* Rows: 0 = Jack, 1 = Bluetooth, 2 = Bluetooth settings shortcut. */
#define N_ROWS   3
static int s_sel;

static void os_enter(screen_t *self)
{
    (void)self;
    /* Start the cursor on the currently active output for quick confirmation. */
    s_sel = (volume_get_output() == VOLUME_OUT_BT) ? 1 : 0;
}

static void os_exit(screen_t *self) { (void)self; }

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    switch (ev) {
    case UI_EVENT_UP:   s_sel = (s_sel - 1 + N_ROWS) % N_ROWS; break;
    case UI_EVENT_DOWN: s_sel = (s_sel + 1) % N_ROWS;         break;
    case UI_EVENT_BACK: navigator_pop();                      break;
    case UI_EVENT_SELECT:
        switch (s_sel) {
        case 0:
            player_set_output(VOLUME_OUT_DAC);
            navigator_pop();
            break;
        case 1:
            /* Inert while nothing is connected; otherwise switch, but keep the popup open if
               the link cannot make the volume safe yet (player_set_output refuses). */
            if (bluetooth_is_connected() &&
                player_set_output(VOLUME_OUT_BT) == ESP_OK) {
                navigator_pop();
            }
            break;
        case 2:
            navigator_push(bluetooth_settings_screen());
            break;
        }
        break;
    default: break;
    }
}

/* One selectable row: caret when selected, label, and a filled dot when it is the active output. */
static void draw_row(int idx, const char *label, gfx_color_t label_col, bool active)
{
    int y = ROW0_Y + idx * ROW_H;
    if (idx == s_sel) gfx_draw_text(CARET_X, y, ">", ACCENT, 1);
    gfx_draw_text(LABEL_X, y, label, label_col, 1);
    if (active) gfx_fill_rect(DOT_X, y + 1, 5, 5, ACCENT);
}

static void render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);

    gfx_fill_rect(BOX_X, BOX_Y, BOX_W, BOX_H, GFX_BLACK);
    gfx_draw_rect(BOX_X, BOX_Y, BOX_W, BOX_H, GFX_WHITE);

    gfx_draw_text(BOX_X + 6, TITLE_Y, "OUTPUT", ACCENT, 1);
    gfx_fill_rect(BOX_X + 6, TITLE_Y + 12, BOX_W - 12, 1, DIM);

    volume_output_t cur = volume_get_output();
    bool bt_ready = bluetooth_is_connected();

    draw_row(0, "Jack", GFX_WHITE, cur == VOLUME_OUT_DAC);
    draw_row(1, "Bluetooth", bt_ready ? GFX_WHITE : DIM, cur == VOLUME_OUT_BT);
    draw_row(2, "BT Settings", GFX_WHITE, false);
}

screen_t *output_select_screen(void)
{
    static screen_t s = {
        .on_enter     = os_enter,
        .on_exit      = os_exit,
        .handle_input = handle_input,
        .render       = render,
    };
    return &s;
}
