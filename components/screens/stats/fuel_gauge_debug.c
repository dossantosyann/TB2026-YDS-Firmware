/**
 * @file fuel_gauge_debug.c
 * @brief Fuel-gauge debug screen: config-vs-expected check, learned state, EZ reload.
 *
 * The config rows compare the gauge's shadow-RAM registers against the values
 * fuel_gauge_init() writes, so a lost configuration (POR) or stale constants show up
 * at a glance. The learned block exposes the capacity the ModelGauge has converged on;
 * a value well above design is the corrupted-learning symptom this page exists to catch.
 * A (behind a confirmation popup) re-runs the EZ model load to purge that state — the
 * cycle counter is preserved, health restarts at 100% by design.
 */
#include "fuel_gauge_debug.h"
#include "navigator.h"
#include "status_bar.h"
#include "gfx.h"
#include "fuel_gauge.h"
#include "power.h"
#include "autonomy.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

#define ACCENT gfx_rgb(255, 120, 0)    /* Stats identity colour (see stats_screen) */
#define DIM    gfx_rgb(110, 110, 110)
#define WARN   gfx_rgb(255, 180, 0)
#define OKC    gfx_rgb(0, 255, 0)
#define BADC   gfx_rgb(255, 60, 60)

#define PAD_X     6
#define TITLE_Y   (STATUS_BAR_H + 4)
#define BODY_Y    (TITLE_Y + 16)
#define LINE_H    12
#define GROUP_GAP 4
#define VAL_X     (PAD_X + 10 * GFX_CHAR_W)   /* value column, after the longest label */
#define STATUS_X  126                          /* OK/BAD verdict column */
#define HINT_Y    162

/* Learned full capacity above this fraction of design = suspicious (corrupted learning). */
#define LEARNED_WARN_FRACTION 1.10f

static int  s_y;            /* current draw row */
static bool s_popup;        /* reload confirmation visible */
static int  s_confirm_sel;  /* 0 = Confirm, 1 = Cancel */

/* Outcome of the last reload attempt, shown until the screen is left. */
static const char *s_msg;
static gfx_color_t s_msg_c;

static void row(const char *label, const char *val, gfx_color_t val_c,
                const char *tok, gfx_color_t tok_c)
{
    gfx_draw_text(PAD_X, s_y, label, DIM, 1);
    gfx_draw_text(VAL_X, s_y, val, val_c, 1);
    if (tok) gfx_draw_text(STATUS_X, s_y, tok, tok_c, 1);
    s_y += LINE_H;
}

/* One config register: value when it matches, "read/want" when it does not. */
static void cfg_row(const char *label, uint16_t raw, uint16_t want, bool hex)
{
    char v[16];
    bool ok = raw == want;
    if (ok) snprintf(v, sizeof v, hex ? "%04X" : "%u", raw);
    else    snprintf(v, sizeof v, hex ? "%04X/%04X" : "%u/%u", raw, want);
    row(label, v, GFX_WHITE, ok ? "OK" : "BAD", ok ? OKC : BADC);
}

static void render_popup(void)
{
    const int bw = 156, bh = 68;
    const int bx = (GFX_W - bw) / 2, by = (GFX_H - bh) / 2;
    gfx_fill_rect(bx, by, bw, bh, GFX_BLACK);
    gfx_draw_rect(bx, by, bw, bh, GFX_WHITE);

    if (autonomy_is_running()) {   /* the test owns the gauge's discharge curve: hands off */
        gfx_draw_text(bx + 6, by + 8,  "Autonomy test running", WARN, 1);
        gfx_draw_text(bx + 6, by + 24, "BACK to close",         DIM,  1);
        return;
    }

    gfx_draw_text(bx + 6, by + 8,  "Reload EZ config?",  ACCENT, 1);
    gfx_draw_text(bx + 6, by + 22, "Keeps cycles. Best", DIM,    1);
    gfx_draw_text(bx + 6, by + 32, "done at rest.",      DIM,    1);

    int confirm_x = bx + 18, cancel_x = bx + 92, oy = by + 50;
    gfx_draw_text((s_confirm_sel == 0 ? confirm_x : cancel_x) - 8, oy, ">", ACCENT, 1);
    gfx_draw_text(confirm_x, oy, "Confirm", GFX_WHITE, 1);
    gfx_draw_text(cancel_x,  oy, "Cancel",  GFX_WHITE, 1);
}

/* A transient SOC can appear while the model reloads, and a false CRITICAL there would
   power the device off mid-action — suspend the auto-shutdown across reload + settle. */
static void do_reload(void)
{
    power_set_low_batt_shutdown(false);
    esp_err_t err = fuel_gauge_reload_ez();
    vTaskDelay(pdMS_TO_TICKS(500));   /* let the first post-reload update land */
    power_set_low_batt_shutdown(true);

    s_msg   = (err == ESP_OK) ? "EZ reload OK" : "EZ reload FAILED";
    s_msg_c = (err == ESP_OK) ? OKC : BADC;
}

static void render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);
    gfx_draw_text(PAD_X, TITLE_Y, "FUEL GAUGE DEBUG", ACCENT, 1);

    fuel_gauge_debug_t d;
    fuel_gauge_data_t  g;
    if (fuel_gauge_read_debug(&d) != ESP_OK || fuel_gauge_read(&g) != ESP_OK) {
        gfx_draw_text(PAD_X, BODY_Y, "gauge read error", BADC, 1);
        return;
    }

    s_y = BODY_Y;
    char v[20];

    /* Config vs what fuel_gauge_init() writes; POR set means the gauge lost it all. */
    cfg_row("DesignCap", d.design_cap_raw, d.design_cap_want, false);
    cfg_row("IChgTerm",  d.ichg_term_raw,  d.ichg_term_want,  false);
    cfg_row("VEmpty",    d.vempty_raw,     d.vempty_want,     true);
    row("POR", d.por ? "set" : "clear", GFX_WHITE,
        d.por ? "BAD" : "OK", d.por ? BADC : OKC);

    s_y += GROUP_GAP;

    /* Learned state: full capacity well above design = corrupted learning. */
    float warn_mah = (float)d.design_cap_want * LEARNED_WARN_FRACTION;
    snprintf(v, sizeof v, "%.0f mAh", d.full_cap_rep_mah);
    row("FullCapR", v, d.full_cap_rep_mah > warn_mah ? WARN : GFX_WHITE, NULL, 0);
    snprintf(v, sizeof v, "%.0f mAh", d.full_cap_nom_mah);
    row("FullCapN", v, d.full_cap_nom_mah > warn_mah ? WARN : GFX_WHITE, NULL, 0);
    snprintf(v, sizeof v, "%.0f mAh", g.rep_cap_mah);
    row("RepCap", v, GFX_WHITE, NULL, 0);
    snprintf(v, sizeof v, "%.1f", g.cycles);
    row("Cycles", v, GFX_WHITE, NULL, 0);

    if (s_msg) {
        s_y += GROUP_GAP;
        gfx_draw_text(PAD_X, s_y, s_msg, s_msg_c, 1);
    }

    gfx_draw_text(PAD_X, HINT_Y, "A: reload EZ config", ACCENT, 1);

    if (s_popup) render_popup();
}

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    if (s_popup) {
        if (autonomy_is_running()) {             /* only way out is to dismiss */
            if (ev == UI_EVENT_BACK || ev == UI_EVENT_SELECT) s_popup = false;
            return;
        }
        switch (ev) {
        case UI_EVENT_UP:
        case UI_EVENT_DOWN:
        case UI_EVENT_LEFT:
        case UI_EVENT_RIGHT: s_confirm_sel ^= 1; break;
        case UI_EVENT_SELECT:
            if (s_confirm_sel == 0) do_reload();
            s_popup = false;
            break;
        case UI_EVENT_BACK:  s_popup = false; break;
        default: break;
        }
        return;
    }

    switch (ev) {
    case UI_EVENT_SELECT: s_popup = true; s_confirm_sel = 1; break;  /* default = Cancel */
    case UI_EVENT_BACK:   navigator_pop(); break;
    default: break;
    }
}

static void on_enter(screen_t *self)
{
    (void)self;
    s_popup = false;
    s_msg   = NULL;
}

static void noop(screen_t *self) { (void)self; }

static screen_t s_screen = {
    .on_enter     = on_enter,
    .on_exit      = noop,
    .handle_input = handle_input,
    .render       = render,
    .refresh_ms   = 500,   /* live values, same cadence as the battery-test screen */
};

screen_t *fuel_gauge_debug_screen(void)
{
    return &s_screen;
}
