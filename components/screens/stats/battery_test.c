/**
 * @file battery_test.c
 * @brief Battery autonomy test screen: pick a workload, check readiness, review the last run.
 *
 * Layout is fixed: the picker and the two setup rows sit above a fixed telemetry block (Batt +
 * last run) and a shared button row. UP/DOWN cycles focus but skips rows that don't apply to the
 * mode and buttons that are greyed out, so the cursor never lands on something it can't act on.
 * The orange < > chevrons flank whichever row is focused.
 *   - Type    : workload picker, the three modes on one line (LEFT/RIGHT cycles Idle/Jack/BT).
 *   - Track   : "Select song" — Jack/BT open the library to pick the song; shows the playing track.
 *   - Sink    : "Connect speaker" — BT opens the Bluetooth screen; shows the connected device.
 *               Both setup rows are always drawn (dim when the mode doesn't use them).
 *   - Start   : arm the run (enabled from a full cell off external power, with a live Jack/BT stream).
 *   - Re-dump : re-export the last run's CSV to the SD card (enabled with a stored run + mounted card).
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "battery_test.h"
#include "storage_screen.h"
#include "bluetooth_settings.h"
#include "navigator.h"
#include "status_bar.h"
#include "gfx.h"
#include "power.h"
#include "player.h"
#include "volume.h"
#include "bluetooth.h"
#include "storage.h"
#include "autonomy.h"

#include <stdio.h>
#include <string.h>

#define ACCENT gfx_rgb(255, 120, 0)    /* Stats identity colour (see stats_screen) */
#define DIM    gfx_rgb(110, 110, 110)
#define WARN   gfx_rgb(255, 180, 0)
#define OKC    gfx_rgb(0, 255, 0)

#define PAD_X    6
#define TITLE_Y  (STATUS_BAR_H + 4)
#define TYPE_Y   34    /* workload picker */
#define SONG_Y   47    /* "Select song" row (Jack/BT), above the speaker row */
#define SINK_Y   60    /* "Connect speaker" row (BT) */
#define READY_Y  75    /* Batt line — this block down (through "SD dump") is fixed on screen */
#define LAST_Y   89
#define L1_Y     100
#define L2_Y     111
#define L3_Y     122
#define L4_Y     133
#define BTN_Y    150   /* START and RE-DUMP SD share this row, side by side */
#define BTN_H    15
#define BTN_GAP  6     /* horizontal gap between the two buttons */

#define NAME_MAX 22    /* device/track names are truncated to fit a centred "< name >" row */

#define READY_SOC_PCT 99.0f   /* a run is only meaningful starting from a full cell */

/* Workload profiles; order matches autonomy_test_t. */
static const char *const k_types[] = { "Idle", "Jack", "BT" };
#define N_TYPES (int)(sizeof k_types / sizeof k_types[0])

/* Focus order top-to-bottom; ZONE_TRACK (Select song) sits above ZONE_SINK (Connect speaker). */
enum { ZONE_TYPE = 0, ZONE_TRACK, ZONE_SINK, ZONE_START, ZONE_REDUMP, N_ZONES };

static int s_type;    /* index into k_types (matches autonomy_test_t) */
static int s_focus;   /* which zone owns LEFT/RIGHT and SELECT */

static bool can_start_now(void);    /* defined below run_ready(); needed by zone_focusable() */
static bool can_redump_now(void);

/* JACK measures whatever is already playing on the jack, so it needs a live DAC stream. */
static bool jack_playing(void)
{
    player_status_t st;
    return player_get_state(&st) == ESP_OK && st.state == PLAYER_PLAYING &&
           volume_get_output() == VOLUME_OUT_DAC;
}

/* BT is ready to measure once a track is playing and a speaker is linked. The current output need
   not be BT already: the run routes the audio to BT itself if it isn't (an A2DP source with no sink
   transmits nothing, so the reading would otherwise be bogus). */
static bool bt_ready(void)
{
    player_status_t st;
    return player_get_state(&st) == ESP_OK && st.state == PLAYER_PLAYING &&
           bluetooth_is_connected();
}

/* Whether the selected workload is ready to start: IDLE always, JACK/BT need their stream live. */
static bool workload_ready(void)
{
    switch (s_type) {
    case AUTONOMY_TEST_JACK: return jack_playing();
    case AUTONOMY_TEST_BT:   return bt_ready();
    default:                 return true;   /* IDLE: nothing to set up */
    }
}

/* Whether focus can land on a zone right now: the setup rows only for the modes that use them
   (Select song = Jack/BT, Connect speaker = BT), and the buttons only while they are enabled — so
   UP/DOWN skip a greyed START/RE-DUMP, and if nothing below is reachable the cursor simply stays. */
static bool zone_focusable(int z)
{
    switch (z) {
    case ZONE_TRACK:  return s_type != AUTONOMY_TEST_IDLE;
    case ZONE_SINK:   return s_type == AUTONOMY_TEST_BT;
    case ZONE_START:  return can_start_now();
    case ZONE_REDUMP: return can_redump_now();
    default:          return true;   /* Type is always reachable, so this loop always terminates */
    }
}

static void focus_step(int dir)
{
    do { s_focus = (s_focus + dir + N_ZONES) % N_ZONES; } while (!zone_focusable(s_focus));
}

/* Glyph run width excluding the cell's trailing inter-char gap (same as power_settings). */
static int text_w(const char *s, int scale)
{
    int n = (int)strlen(s);
    return n ? n * GFX_CHAR_W * scale - scale : 0;
}

/* Full cell width of a string (each glyph advances one GFX_CHAR_W cell), for laying tokens in a
   row: unlike text_w() it keeps the trailing gap, so tokens drawn back-to-back keep even spacing. */
static int cell_w(const char *s) { return (int)strlen(s) * GFX_CHAR_W; }

/* Right-truncate with "..." into dst (size must be max_chars+1 or more). */
static void truncate_right(char *dst, size_t dst_sz, const char *src, int max_chars)
{
    if ((int)strlen(src) <= max_chars) snprintf(dst, dst_sz, "%s", src);
    else                               snprintf(dst, dst_sz, "%.*s...", max_chars - 3, src);
}

static void draw_button(int x, int y, int w, const char *label, bool focused, bool enabled)
{
    gfx_color_t c = !enabled ? DIM : (focused ? ACCENT : GFX_WHITE);
    gfx_draw_rect(x, y, w, BTN_H, c);
    /* Glyph ink is GFX_CHAR_H-1 tall (the cell adds a 1px gap at the bottom); centre on the
       ink, not the cell, or the text sits a pixel high. Same fix as power_settings. */
    int ty = y + (BTN_H - (GFX_CHAR_H - 1)) / 2;
    gfx_draw_text(x + (w - text_w(label, 1)) / 2, ty, label, c, 1);
}

/* Type row: the three workloads on one centred line. The selected one is white, the others dim.
   Only when the row is focused does the selected one get orange < > chevrons (the LEFT/RIGHT cue). */
static void draw_type_row(int y, bool focused)
{
    int w = 0;
    for (int i = 0; i < N_TYPES; i++) {
        if (i) w += GFX_CHAR_W;                            /* one space between workloads */
        if (focused && i == s_type) w += 2 * GFX_CHAR_W;   /* the < and > around the selected one */
        w += cell_w(k_types[i]);
    }
    int x = (GFX_W - w) / 2;
    for (int i = 0; i < N_TYPES; i++) {
        if (i) x += GFX_CHAR_W;
        if (focused && i == s_type) {
            gfx_draw_text(x, y, "<", ACCENT, 1);            x += GFX_CHAR_W;
            gfx_draw_text(x, y, k_types[i], GFX_WHITE, 1);  x += cell_w(k_types[i]);
            gfx_draw_text(x, y, ">", ACCENT, 1);            x += GFX_CHAR_W;
        } else {
            gfx_draw_text(x, y, k_types[i], i == s_type ? GFX_WHITE : DIM, 1);
            x += cell_w(k_types[i]);
        }
    }
}

/* Setup row (Select song / Connect speaker): always shown, centred; white when the current mode
   uses it, dim otherwise. Shows the current selection (track / device name) when set, else the
   prompt. Only a focused (hence applicable) row gains the orange < > chevrons (the SELECT cue). */
static void draw_pick_row(int y, const char *text, bool applicable, bool focused)
{
    gfx_color_t tc  = applicable ? GFX_WHITE : DIM;
    int side = focused ? 2 * GFX_CHAR_W : 0;   /* "< " on the left, " >" on the right */
    int x    = (GFX_W - (cell_w(text) + 2 * side)) / 2;
    if (focused) gfx_draw_text(x, y, "<", ACCENT, 1);
    gfx_draw_text(x + side, y, text, tc, 1);
    if (focused) gfx_draw_text(x + side + cell_w(text) + GFX_CHAR_W, y, ">", ACCENT, 1);
}

/* A run must discharge the battery, so it starts only from a full cell AND off external power
   (USB/charger latched on would both stop the discharge and prevent the auto-shutdown end). */
static bool run_ready(const power_state_t *p)
{
    return p->valid && !p->external_power && p->soc_pct >= READY_SOC_PCT;
}

/* START is enabled from a full cell off external power with the workload's stream live; RE-DUMP is
   enabled once a run is stored and a card is mounted. Also drive which buttons focus can land on. */
static bool can_start_now(void)
{
    power_state_t p;
    power_get_state(&p);
    return run_ready(&p) && workload_ready();
}

static bool can_redump_now(void)
{
    return autonomy_get_last_result() != AUTONOMY_RESULT_NONE && storage_ready();
}

static void on_enter(screen_t *self) { (void)self; s_focus = ZONE_TYPE; }

static void render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);
    gfx_draw_text(PAD_X, TITLE_Y, "BATTERY TEST", ACCENT, 1);

    /* Workload picker. */
    draw_type_row(TYPE_Y, s_focus == ZONE_TYPE);

    /* Both setup rows are always shown at fixed positions: "Select song" (Jack/BT) above "Connect
       speaker" (BT), white when the mode uses the row and dim otherwise. Each shows the current
       selection (track / device name) when set, else the prompt. */
    bool song_on = (s_type != AUTONOMY_TEST_IDLE);
    const char *song = "Select song";
    char song_nm[NAME_MAX + 4];
    if (song_on) {
        player_status_t st;
        if (player_get_state(&st) == ESP_OK && st.state == PLAYER_PLAYING && st.track.name) {
            truncate_right(song_nm, sizeof song_nm, st.track.name, NAME_MAX);
            song = song_nm;
        }
    }
    draw_pick_row(SONG_Y, song, song_on, s_focus == ZONE_TRACK);

    bool sink_on = (s_type == AUTONOMY_TEST_BT);
    const char *sink = "Connect speaker";
    char sink_nm[NAME_MAX + 4];
    if (sink_on && bluetooth_is_connected()) {
        bt_known_device_t d;
        truncate_right(sink_nm, sizeof sink_nm, bluetooth_get_last_device(&d) ? d.name : "speaker", NAME_MAX);
        sink = sink_nm;
    }
    draw_pick_row(SINK_Y, sink, sink_on, s_focus == ZONE_SINK);

    /* Readiness: live SOC, gated at READY_SOC_PCT and off external power. */
    power_state_t p;
    power_get_state(&p);
    int  pct = (int)(p.soc_pct + 0.5f);
    char rb[36];
    gfx_color_t rc;
    if (!p.valid)              { snprintf(rb, sizeof rb, "Batt:  --");                      rc = DIM;  }
    else if (p.external_power) { snprintf(rb, sizeof rb, "Batt: %d%%  unplug to start", pct); rc = WARN; }
    else if (run_ready(&p))    { snprintf(rb, sizeof rb, "Batt: %d%%  READY", pct);           rc = OKC;  }
    else                       { snprintf(rb, sizeof rb, "Batt: %d%%  charge to 100%%", pct); rc = WARN; }
    gfx_draw_text(PAD_X, READY_Y, rb, rc, 1);

    /* Last-run telemetry. */
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

    /* START and RE-DUMP SD share one row, each in half the width, greyed when unavailable. */
    int bw = (GFX_W - 2 * PAD_X - BTN_GAP) / 2;
    draw_button(PAD_X,               BTN_Y, bw, "START",      s_focus == ZONE_START,  can_start_now());
    draw_button(PAD_X + bw + BTN_GAP, BTN_Y, bw, "RE-DUMP SD", s_focus == ZONE_REDUMP, can_redump_now());
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
        if (s_focus == ZONE_SINK) {
            navigator_push(bluetooth_settings_screen());   /* connect the speaker here */
        } else if (s_focus == ZONE_TRACK) {
            navigator_push(storage_screen());              /* pick/play the song from the library */
        } else if (s_focus == ZONE_START) {
            if (can_start_now())
                autonomy_start((autonomy_test_t)s_type);   /* k_types order matches autonomy_test_t */
        } else if (s_focus == ZONE_REDUMP) {
            if (can_redump_now())
                autonomy_redump();   /* re-export the last stored run to the SD card (confirm the path) */
        }
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
