/**
 * @file audio_settings.c
 * @brief Audio settings: output routing (top) and an L/R balance slider (bottom).
 *
 * Two stacked sections below the status bar, like the Bluetooth screen. The top mirrors the
 * Now-Playing output popup (Jack / Bluetooth / BT Settings) but in the Settings red accent, and
 * applies the choice in place -- the active output keeps its filled dot, the Bluetooth row is
 * dimmed and inert while nothing is connected. The bottom is a centred balance slider: pushing
 * it left makes the LEFT channel louder, right the RIGHT one, up to +20 dB per side.
 *
 * The slider drives the DAC L/R trim live (volume_set_balance) for immediate feedback; the value
 * is persisted to NVS once, on screen exit (volume_save_balance), and restored by volume_init at
 * boot -- so a still slider costs no flash writes.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "audio_settings.h"
#include "bluetooth_settings.h"
#include "navigator.h"
#include "status_bar.h"
#include "gfx.h"
#include "player.h"
#include "volume.h"
#include "bluetooth.h"

#include <stdio.h>
#include <stdlib.h>

/* ---- layout ----------------------------------------------------------------------
 * The content area (below the status bar) is split into two equal halves by a centred
 * separator; each section's group of elements is then vertically centred in its half. */

#define CONTENT_TOP (STATUS_BAR_H)
#define CONTENT_BOT (GFX_H)
#define MID_Y       ((CONTENT_TOP + CONTENT_BOT) / 2)      /* separator, at the middle */

#define ROW_H       16
#define CARET_X     4
#define LABEL_X     16
#define DOT_X       (GFX_W - 14)               /* active-output marker, right-aligned */

/* Top half: "OUTPUT" header (one line) + 3 rows, centred in [CONTENT_TOP, MID_Y]. */
#define OUT_GROUP_H (16 + 3 * ROW_H)
#define OUT_HDR_Y   (CONTENT_TOP + ((MID_Y - CONTENT_TOP) - OUT_GROUP_H) / 2)
#define ROW0_Y      (OUT_HDR_Y + 16)           /* first output row */

#define SEP_Y       MID_Y                      /* separator between the two sections */

/* Bottom half: "BALANCE" header + track + numeric readout, centred in [MID_Y, CONTENT_BOT]. */
#define BAL_GROUP_H 50
#define BAL_HDR_Y   (MID_Y + ((CONTENT_BOT - MID_Y) - BAL_GROUP_H) / 2)
#define BAR_X       18
#define BAR_W       140
#define BAR_Y       (BAL_HDR_Y + 24)
#define BAR_H       6
#define VAL_Y       (BAR_Y + 18)               /* numeric readout under the bar */

#define ACCENT      gfx_rgb(255, 0, 0)         /* Settings identity colour (see root_menu) */
#define DIM         gfx_rgb(110, 110, 110)

/* Selectable rows: three output choices then the slider. */
enum { SEL_JACK = 0, SEL_BT, SEL_BT_SETTINGS, SEL_SLIDER, N_SEL };

#define BAL_MAX     40                          /* 40 steps * 0.5 dB = +20 dB per side */

static int s_sel;
static int s_steps;   /* live balance: >0 boosts LEFT (attenuates R), <0 boosts RIGHT */

/* ---- output section -------------------------------------------------------------- */

static void draw_out_row(int idx, const char *label, gfx_color_t label_col, bool active)
{
    int y = ROW0_Y + idx * ROW_H;
    if (idx == s_sel) gfx_draw_text(CARET_X, y, ">", ACCENT, 1);
    gfx_draw_text(LABEL_X, y, label, label_col, 1);
    if (active) gfx_fill_rect(DOT_X, y + 1, 5, 5, ACCENT);
}

/* ---- balance section ------------------------------------------------------------- */

/* "L +6.5 dB", "R +3.0 dB", or "Centered" (steps are half-dB each). */
static void balance_label(char *buf, size_t n)
{
    if (s_steps == 0) { snprintf(buf, n, "Centered"); return; }
    int mag = abs(s_steps);                     /* half-dB steps -> whole.fraction */
    snprintf(buf, n, "%c +%d.%d dB", s_steps > 0 ? 'L' : 'R', mag / 2, (mag % 2) * 5);
}

static void draw_slider(void)
{
    bool selected = (s_sel == SEL_SLIDER);

    gfx_draw_text(CARET_X, BAL_HDR_Y, selected ? ">" : " ", ACCENT, 1);
    gfx_draw_text(LABEL_X, BAL_HDR_Y, "BALANCE", DIM, 1);

    /* L / R endpoint hints framing the track. */
    gfx_draw_text(BAR_X - 10, BAR_Y - 1, "L", DIM, 1);
    gfx_draw_text(BAR_X + BAR_W + 4, BAR_Y - 1, "R", DIM, 1);

    /* Track with a centre tick. */
    gfx_fill_rect(BAR_X, BAR_Y + BAR_H / 2 - 1, BAR_W, 2, DIM);
    int cx = BAR_X + BAR_W / 2;
    gfx_fill_rect(cx, BAR_Y - 2, 1, BAR_H + 4, DIM);

    /* Knob: positive steps (louder L) sit to the LEFT of centre. */
    int knob = cx - s_steps * (BAR_W / 2) / BAL_MAX;
    gfx_fill_rect(knob - 2, BAR_Y - 3, 5, BAR_H + 6, selected ? ACCENT : GFX_WHITE);

    char buf[16];
    balance_label(buf, sizeof buf);
    gfx_draw_text(BAR_X, VAL_Y, buf, GFX_WHITE, 1);
}

/* ---- render ---------------------------------------------------------------------- */

static void render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);

    gfx_draw_text(LABEL_X, OUT_HDR_Y, "OUTPUT", DIM, 1);

    volume_output_t cur = volume_get_output();
    bool bt_ready = bluetooth_is_connected();
    draw_out_row(SEL_JACK,        "Jack",        GFX_WHITE,             cur == VOLUME_OUT_DAC);
    draw_out_row(SEL_BT,          "Bluetooth",   bt_ready ? GFX_WHITE : DIM, cur == VOLUME_OUT_BT);
    draw_out_row(SEL_BT_SETTINGS, "BT Settings", GFX_WHITE,             false);

    gfx_fill_rect(BAR_X, SEP_Y, GFX_W - 2 * BAR_X, 1, DIM);

    draw_slider();
}

/* ---- input ----------------------------------------------------------------------- */

static void activate(void)
{
    switch (s_sel) {
    case SEL_JACK:
        player_set_output(VOLUME_OUT_DAC);
        break;
    case SEL_BT:
        /* Inert while nothing is connected (an un-acked speaker volume only defers the
           stream, it does not fail the call). */
        if (bluetooth_is_connected()) player_set_output(VOLUME_OUT_BT);
        break;
    case SEL_BT_SETTINGS:
        navigator_push(bluetooth_settings_screen());
        break;
    default: break;
    }
}

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    switch (ev) {
    case UI_EVENT_UP:   s_sel = (s_sel - 1 + N_SEL) % N_SEL; break;
    case UI_EVENT_DOWN: s_sel = (s_sel + 1) % N_SEL;        break;
    case UI_EVENT_LEFT:
        if (s_sel == SEL_SLIDER && s_steps < BAL_MAX) {
            s_steps++;                          /* toward L: boost left, attenuate right */
            volume_set_balance((int8_t)s_steps);
        }
        break;
    case UI_EVENT_RIGHT:
        if (s_sel == SEL_SLIDER && s_steps > -BAL_MAX) {
            s_steps--;
            volume_set_balance((int8_t)s_steps);
        }
        break;
    case UI_EVENT_SELECT: activate();      break;
    case UI_EVENT_BACK:   navigator_pop(); break;
    default: break;
    }
}

/* ---- lifecycle ------------------------------------------------------------------- */

static void audio_on_enter(screen_t *self)
{
    (void)self;
    s_steps = volume_get_balance();
    /* Start the cursor on the active output for quick confirmation. */
    s_sel = (volume_get_output() == VOLUME_OUT_BT) ? SEL_BT : SEL_JACK;
}

static void audio_on_exit(screen_t *self)
{
    (void)self;
    volume_save_balance();                  /* persist once, on the way out */
}

screen_t *audio_settings_screen(void)
{
    static screen_t s = {
        .on_enter     = audio_on_enter,
        .on_exit      = audio_on_exit,
        .handle_input = handle_input,
        .render       = render,
    };
    return &s;
}
