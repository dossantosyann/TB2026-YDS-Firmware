/**
 * @file battery_test.c
 * @brief Battery autonomy test screen: pick a workload, check readiness, review the last run.
 *
 * Interface only for now — the run logic (workload setup, periodic checkpoint to NVS, SD CSV
 * export on the next boot) is not wired yet, so START and RE-DUMP are no-ops. The readiness
 * line is live (reads the cached fuel-gauge snapshot) so the gating already reflects reality:
 * a run should only start from a full cell.
 *
 * Three focusable zones (UP/DOWN cycles focus):
 *   - Type    : workload picker (LEFT/RIGHT cycles Idle/Jack/BT).
 *   - Start   : arm the run (gated on battery == 100%).
 *   - Re-dump : re-export the last run's CSV to the SD card, to confirm the write path.
 * The last-run block is read-only telemetry (all "--" until a run has completed).
 */
#include "battery_test.h"
#include "storage_screen.h"
#include "navigator.h"
#include "status_bar.h"
#include "gfx.h"
#include "power.h"
#include "player.h"
#include "volume.h"
#include "autonomy.h"

#include <stdio.h>
#include <string.h>

#define ACCENT gfx_rgb(255, 120, 0)    /* Stats identity colour (see stats_screen) */
#define DIM    gfx_rgb(110, 110, 110)
#define WARN   gfx_rgb(255, 180, 0)
#define OKC    gfx_rgb(0, 255, 0)

#define PAD_X    6
#define TITLE_Y  (STATUS_BAR_H + 4)
#define TYPE_Y   34
#define PLAY_Y   48    /* Jack only: current-playback status line */
#define READY_Y  62
#define LAST_Y   78
#define L1_Y     90
#define L2_Y     101
#define L3_Y     112
#define L4_Y     123
#define START_Y  138
#define REDUMP_Y 157
#define BTN_H    15
#define VAL_X    60    /* column where the Type value starts */

#define READY_SOC_PCT 50.5f   /* a run is only meaningful starting from a full cell */

/* Workload profiles; order matches autonomy_test_t. */
static const char *const k_types[] = { "Idle", "Jack", "BT" };
#define N_TYPES (int)(sizeof k_types / sizeof k_types[0])

enum { ZONE_TYPE = 0, ZONE_TRACK, ZONE_START, ZONE_REDUMP, N_ZONES };

static int s_type;    /* index into k_types (matches autonomy_test_t) */
static int s_focus;   /* which zone owns LEFT/RIGHT and SELECT */

/* JACK measures whatever is already playing on the jack, so it needs a live DAC stream. */
static bool jack_playing(void)
{
    player_status_t st;
    return player_get_state(&st) == ESP_OK && st.state == PLAYER_PLAYING &&
           volume_get_output() == VOLUME_OUT_DAC;
}

/* The Track zone (opens the library to start/change the song) only applies to Jack. */
static bool zone_active(int z) { return z != ZONE_TRACK || s_type == AUTONOMY_TEST_JACK; }

static void focus_step(int dir)
{
    do { s_focus = (s_focus + dir + N_ZONES) % N_ZONES; } while (!zone_active(s_focus));
}

/* Glyph run width excluding the cell's trailing inter-char gap (same as power_settings). */
static int text_w(const char *s, int scale)
{
    int n = (int)strlen(s);
    return n ? n * GFX_CHAR_W * scale - scale : 0;
}

/* Right-truncate with "..." into dst (size must be max_chars+1 or more). */
static void truncate_right(char *dst, size_t dst_sz, const char *src, int max_chars)
{
    if ((int)strlen(src) <= max_chars) snprintf(dst, dst_sz, "%s", src);
    else                               snprintf(dst, dst_sz, "%.*s...", max_chars - 3, src);
}

static void draw_button(int y, const char *label, bool focused, bool enabled)
{
    int bw = text_w(label, 1) + 16;
    int bx = (GFX_W - bw) / 2;
    gfx_color_t c = !enabled ? DIM : (focused ? ACCENT : GFX_WHITE);
    gfx_draw_rect(bx, y, bw, BTN_H, c);
    /* Glyph ink is GFX_CHAR_H-1 tall (the cell adds a 1px gap at the bottom); centre on the
       ink, not the cell, or the text sits a pixel high. Same fix as power_settings. */
    int ty = y + (BTN_H - (GFX_CHAR_H - 1)) / 2;
    gfx_draw_text(bx + (bw - text_w(label, 1)) / 2, ty, label, c, 1);
}

/* A run must discharge the battery, so it starts only from a full cell AND off external power
   (USB/charger latched on would both stop the discharge and prevent the auto-shutdown end). */
static bool run_ready(const power_state_t *p)
{
    return p->valid && !p->external_power && p->soc_pct >= READY_SOC_PCT;
}

static void on_enter(screen_t *self) { (void)self; s_focus = ZONE_TYPE; }

static void render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);
    gfx_draw_text(PAD_X, TITLE_Y, "BATTERY TEST", ACCENT, 1);

    /* Workload picker. */
    bool ft = (s_focus == ZONE_TYPE);
    gfx_draw_text(PAD_X, TYPE_Y, "Type:", ft ? ACCENT : DIM, 1);
    if (ft) gfx_draw_text(VAL_X - 12, TYPE_Y, "<", ACCENT, 1);
    gfx_draw_text(VAL_X, TYPE_Y, k_types[s_type], GFX_WHITE, 1);
    if (ft) gfx_draw_text(VAL_X + text_w(k_types[s_type], 1) + 6, TYPE_Y, ">", ACCENT, 1);

    /* Jack: a focusable row that opens the existing library to start/change the track.
       The run measures whatever is playing on the jack, so show it (or prompt to open it). */
    bool play_ok = (s_type == AUTONOMY_TEST_JACK) && jack_playing();
    if (s_type == AUTONOMY_TEST_JACK) {
        bool fr = (s_focus == ZONE_TRACK);
        gfx_draw_text(PAD_X, PLAY_Y, "Track:", fr ? ACCENT : DIM, 1);
        if (play_ok) {
            player_status_t st;
            player_get_state(&st);
            char pb[24];
            truncate_right(pb, sizeof pb, st.track.name ? st.track.name : "", 18);
            gfx_draw_text(VAL_X, PLAY_Y, pb, GFX_WHITE, 1);
        } else {
            gfx_draw_text(VAL_X, PLAY_Y, "open library", fr ? ACCENT : WARN, 1);
        }
    }

    /* Readiness: live SOC, gated at 100% and off external power. */
    power_state_t p;
    power_get_state(&p);
    bool ready = run_ready(&p);
    int  pct = (int)(p.soc_pct + 0.5f);
    char rb[36];
    gfx_color_t rc;
    if (!p.valid)              { snprintf(rb, sizeof rb, "Batt:  --");                      rc = DIM;  }
    else if (p.external_power) { snprintf(rb, sizeof rb, "Batt: %d%%  unplug to start", pct); rc = WARN; }
    else if (ready)            { snprintf(rb, sizeof rb, "Batt: %d%%  READY", pct);           rc = OKC;  }
    else                       { snprintf(rb, sizeof rb, "Batt: %d%%  charge to 100%%", pct); rc = WARN; }
    gfx_draw_text(PAD_X, READY_Y, rb, rc, 1);

    /* Last-run telemetry. Type/Result come from the service; Time and SD dump land in Phase B. */
    autonomy_result_t res = autonomy_get_last_result();
    const char *type_s = "--", *res_s = "--";
    gfx_color_t res_c = DIM;
    if (res != AUTONOMY_RESULT_NONE) {
        type_s = k_types[autonomy_get_last_type()];
        if      (res == AUTONOMY_RESULT_CANCELLED)   { res_s = "CANCELLED";   res_c = WARN; }
        else if (res == AUTONOMY_RESULT_ABORTED)     { res_s = "ABORTED";     res_c = gfx_rgb(255, 60, 60); }
        else if (res == AUTONOMY_RESULT_INTERRUPTED) { res_s = "INTERRUPTED"; res_c = WARN; }
        else                                         { res_s = "OK";          res_c = OKC; }
    }
    char lb[24];
    gfx_draw_text(PAD_X, LAST_Y, "Last run:", GFX_WHITE, 1);
    snprintf(lb, sizeof lb, "  Type    %s", type_s);
    gfx_draw_text(PAD_X, L1_Y, lb, DIM, 1);
    gfx_draw_text(PAD_X, L2_Y, "  Result", DIM, 1);
    gfx_draw_text(PAD_X + 10 * GFX_CHAR_W, L2_Y, res_s, res_c, 1);

    if (res != AUTONOMY_RESULT_NONE) {
        uint32_t s = autonomy_get_last_duration_s(), h = s / 3600, m = (s % 3600) / 60;
        if (h) snprintf(lb, sizeof lb, "  Time    %uh%02um", (unsigned)h, (unsigned)m);
        else   snprintf(lb, sizeof lb, "  Time    %um", (unsigned)m);
    } else {
        snprintf(lb, sizeof lb, "  Time    --");
    }
    gfx_draw_text(PAD_X, L3_Y, lb, DIM, 1);

    const char *sd_s = "--";
    gfx_color_t sd_c = DIM;
    if (res != AUTONOMY_RESULT_NONE) {   /* cancelled, completed and aborted runs are all written */
        if (autonomy_get_last_dump_ok()) { sd_s = "OK";      sd_c = OKC;  }
        else                             { sd_s = "pending"; sd_c = WARN; }
    }
    gfx_draw_text(PAD_X, L4_Y, "  SD dump", DIM, 1);
    gfx_draw_text(PAD_X + 10 * GFX_CHAR_W, L4_Y, sd_s, sd_c, 1);

    bool can_start = ready && (s_type != AUTONOMY_TEST_JACK || play_ok);
    draw_button(START_Y,  "START",      s_focus == ZONE_START,  can_start);
    draw_button(REDUMP_Y, "RE-DUMP SD", s_focus == ZONE_REDUMP, true);
}

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    switch (ev) {
    case UI_EVENT_UP:   focus_step(-1); break;
    case UI_EVENT_DOWN: focus_step(+1); break;
    case UI_EVENT_LEFT:
        if (s_focus == ZONE_TYPE) s_type = (s_type - 1 + N_TYPES) % N_TYPES;
        break;
    case UI_EVENT_RIGHT:
        if (s_focus == ZONE_TYPE) s_type = (s_type + 1) % N_TYPES;
        break;
    case UI_EVENT_SELECT:
        if (s_focus == ZONE_TRACK) {
            navigator_push(storage_screen());   /* the existing library: play/change the track here */
        } else if (s_focus == ZONE_START) {
            power_state_t p;
            power_get_state(&p);
            if (run_ready(&p) && (s_type != AUTONOMY_TEST_JACK || jack_playing()))
                autonomy_start((autonomy_test_t)s_type);   /* k_types order matches autonomy_test_t */
        }
        /* TODO: ZONE_REDUMP re-exports the last CSV (Phase E). */
        break;
    case UI_EVENT_BACK: navigator_pop(); break;
    default: break;
    }
}

static void noop(screen_t *self) { (void)self; }

static screen_t s_screen = {
    .on_enter     = on_enter,
    .on_exit      = noop,
    .handle_input = handle_input,
    .render       = render,
    .refresh_ms   = 500,   /* keep the readiness line live */
};

screen_t *battery_test_screen(void)
{
    return &s_screen;
}
