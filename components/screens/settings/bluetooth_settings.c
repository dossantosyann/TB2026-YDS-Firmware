/**
 * @file bluetooth_settings.c
 * @brief Bluetooth settings: one combined screen listing paired and nearby devices.
 *
 * Layout (below the always-present status bar): a "KNOWN DEVICES" section from the persisted known
 * list (most-recent first, so the last device sits on top), then a "NEARBY" section with a "Scan"
 * action and the live scan results (signal shown as bars + dBm). A caret ">" marks
 * the selection -- no full white bar, which on this passive OLED lights OFF pixels across the line.
 *
 * Power: the radio is off by default. Entering this screen shows the paired list straight from NVS
 * with the radio down. It is powered up (bluetooth_init) only when the user scans or connects, and
 * powered down again (bluetooth_shutdown) on exit unless a link is up -- so it draws current only
 * while scanning or connected, per the project's power rule. While the screen is open it also
 * suppresses the service's auto power-down on link drop (see bluetooth_set_auto_shutdown), so a
 * manual disconnect keeps the radio up for the next action; outside the screen the service powers
 * itself down when the link drops.
 *
 * Selecting a nearby device connects to it. Selecting a paired device opens a small popup with
 * Connect/Disconnect and Forget.
 */
#include "bluetooth_settings.h"
#include "navigator.h"
#include "status_bar.h"
#include "gfx.h"
#include "bluetooth.h"
#include "player.h"
#include "volume.h"

#include <string.h>

/* ---- layout ---------------------------------------------------------------------- */

#define CONTENT_Y0   (STATUS_BAR_H + 2)
#define LINE_H       14
#define CARET_X      3
#define TEXT_X       13
#define NAME_MAXCH   20                 /* name chars drawn before the signal column */
#define SIGNAL_X     150                /* signal bars / connected dot, at the right margin */
#define REFRESH_MS   250                /* live re-render cadence while scanning/connecting */

#define ACCENT       gfx_rgb(255, 0, 0) /* Settings identity colour (see root_menu) */
#define DIM          gfx_rgb(110, 110, 110)

/* ---- selectable-row model -------------------------------------------------------- */

typedef enum { ROW_KNOWN, ROW_SCAN, ROW_NEAR } row_kind_t;

/* Snapshots rebuilt each frame from the service (both under its own lock). */
static bt_known_device_t  s_known[BLUETOOTH_MAX_KNOWN];
static size_t             s_nknown;
static bluetooth_device_t s_near[BLUETOOTH_MAX_DEVICES];
static size_t             s_nnear;

static int  s_sel;        /* index over selectable rows: [known..][scan][near..] */
static int  s_top;        /* first visible visual line (for scrolling) */
static bool s_powered;    /* the radio is up (we called bluetooth_init this session) */

/* A connect we issued from this screen, so we can flag its row as "connecting" (known or nearby). */
static bool          s_connecting;
static esp_bd_addr_t s_connecting_bda;
static bool          s_blink;      /* toggles each rendered frame -> the connecting dot blinks */
static const char   *s_error;      /* last failed action, drawn on the bottom line (NULL = none) */

/* Device-options popup */
static bool               s_popup;
static bt_known_device_t  s_popup_dev;
static int                s_popup_sel;   /* 0 = Connect/Disconnect, 1 = Forget */

/* Scan-confirmation popup (only when a scan would interrupt Bluetooth playback) */
static bool               s_scan_confirm;
static int                s_confirm_sel; /* 0 = OK, 1 = Cancel */

/* ---- helpers --------------------------------------------------------------------- */

static void reload(void)
{
    s_nknown = bluetooth_get_known_devices(s_known, BLUETOOTH_MAX_KNOWN);
    s_nnear  = s_powered ? bluetooth_get_devices(s_near, BLUETOOTH_MAX_DEVICES) : 0;
}

static int sel_count(void)
{
    return (int)s_nknown + 1 /* scan */ + (int)s_nnear;
}

/* Decode a selectable index into (kind, sub-index). */
static row_kind_t sel_kind(int sel, int *idx)
{
    if (sel < (int)s_nknown)         { *idx = sel;                     return ROW_KNOWN; }
    if (sel == (int)s_nknown)        { *idx = 0;                       return ROW_SCAN;  }
    *idx = sel - (int)s_nknown - 1;                                    return ROW_NEAR;
}

/* True if @p bda is the currently connected device (the connected one is always known[0]). */
static bool is_connected_dev(const esp_bd_addr_t bda)
{
    bt_known_device_t last;
    return bluetooth_is_connected() && bluetooth_get_last_device(&last) &&
           memcmp(last.bda, bda, ESP_BD_ADDR_LEN) == 0;
}

/* True if @p bda is the device this screen is currently connecting to (gated on the live state, so
   the flag self-clears once the attempt resolves to CONNECTED/FAILED). */
static bool is_connecting_dev(const esp_bd_addr_t bda)
{
    return s_connecting &&
           bluetooth_get_conn_state() == BLUETOOTH_CONN_CONNECTING &&
           memcmp(s_connecting_bda, bda, ESP_BD_ADDR_LEN) == 0;
}

/* Ensure the radio is up before a scan/connect. False (with s_error set) when the stack failed
   to come up -- callers must then skip the scan/connect instead of firing it at a dead radio. */
static bool power_on(void)
{
    if (!s_powered) {
        if (bluetooth_init() == ESP_OK) s_powered = true;
        else                            s_error = "Bluetooth init failed";
    }
    return s_powered;
}

/* True only when audio the user hears is actually going over Bluetooth (BT is the active output and
   a track is playing). A mere idle A2DP link does not count -- scanning it back has no audio impact,
   and it must NOT pause wired jack playback. */
static bool bt_audio_active(void)
{
    player_status_t st;
    return volume_get_output() == VOLUME_OUT_BT &&
           player_get_state(&st) == ESP_OK && st.state == PLAYER_PLAYING;
}

/* Start an inquiry. If audio is streaming over Bluetooth, pause it first: the inquiry floods the
   radio and would otherwise break the A2DP stream mid-note. */
static void start_scan(void)
{
    if (bt_audio_active()) player_pause();
    s_error      = NULL;
    s_connecting = false;              /* a new action clears a stale "Connection failed" line */
    if (power_on()) bluetooth_scan_start();
}

/* ---- rendering ------------------------------------------------------------------- */

/* One name, truncated to NAME_MAXCH characters, "(inconnu)" when empty. */
static void draw_name(int x, int y, const char *name, gfx_color_t col)
{
    char buf[NAME_MAXCH + 1];
    if (!name || !name[0]) name = "(unknown)";
    size_t n = strnlen(name, NAME_MAXCH);
    memcpy(buf, name, n);
    buf[n] = '\0';
    gfx_draw_text(x, y, buf, col, 1);
}

/* Signal as up-to-4 bars from RSSI dBm; INT8_MIN means unknown. */
static void draw_signal(int y, int8_t rssi)
{
    if (rssi == INT8_MIN) { gfx_draw_text(SIGNAL_X, y, "?", DIM, 1); return; }

    int level = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : rssi >= -85 ? 1 : 0;
    for (int i = 0; i < 4; i++) {
        int h  = 2 + i * 2;                 /* 2,4,6,8 px tall */
        int bx = SIGNAL_X + i * 3;
        int by = y + 8 - h;                 /* bottom-aligned to the text baseline */
        gfx_fill_rect(bx, by, 2, h, i < level ? GFX_WHITE : DIM);
    }
}

/* "Connecting" marker: an amber dot blinking at the refresh cadence, in the signal column. */
static void draw_connecting(int y)
{
    gfx_fill_rect(SIGNAL_X, y + 2, 4, 4, s_blink ? gfx_rgb(255, 170, 0) : DIM);
}

/* A section header line (non-selectable), consuming one visual line at @p y. */
static void draw_header(int y, const char *text)
{
    gfx_draw_text(TEXT_X, y, text, DIM, 1);
}

/* Draw the whole scrolling list. Visual lines are: header, known items, header, scan, near items.
   We render into a virtual line index, then only paint those within the visible window. */
static void render_list(screen_t *self)
{
    gfx_clear(GFX_BLACK);

    const int visible = (GFX_H - CONTENT_Y0) / LINE_H;

    /* Locate the selected item's visual line so we can keep it in view. Visual line numbering:
       0 = "KNOWN DEVICES" header, 1..nknown = known, then a separator line, "NEARBY" header,
       scan, near items. */
    int sel_vline;
    {
        int idx; row_kind_t k = sel_kind(s_sel, &idx);
        if (k == ROW_KNOWN)     sel_vline = 1 + idx;
        else if (k == ROW_SCAN) sel_vline = (int)s_nknown + 3;         /* +separator +NEARBY header */
        else                    sel_vline = (int)s_nknown + 4 + idx;
    }
    if (sel_vline < s_top)               s_top = sel_vline;
    if (sel_vline > s_top + visible - 1) s_top = sel_vline - visible + 1;

    int vline = 0;
    #define ROW_Y(v)  (CONTENT_Y0 + ((v) - s_top) * LINE_H)
    #define SHOWN(v)  ((v) >= s_top && (v) < s_top + visible)

    /* KNOWN DEVICES */
    if (SHOWN(vline)) draw_header(ROW_Y(vline), "KNOWN DEVICES");
    vline++;
    for (size_t i = 0; i < s_nknown; i++, vline++) {
        if (!SHOWN(vline)) continue;
        int y = ROW_Y(vline);
        bool selected = (s_sel == (int)i);
        if (selected) gfx_draw_text(CARET_X, y, ">", ACCENT, 1);
        draw_name(TEXT_X, y, s_known[i].name, GFX_WHITE);
        /* Status dot: amber (blinking) while connecting, else green connected / red not. */
        if (is_connecting_dev(s_known[i].bda))
            draw_connecting(y);
        else
            gfx_fill_rect(SIGNAL_X, y + 2, 4, 4,
                          is_connected_dev(s_known[i].bda) ? gfx_rgb(0, 220, 0) : gfx_rgb(220, 0, 0));
    }

    /* Separator: its own row, so the grey line has padding above and below (centred vertically). */
    if (SHOWN(vline))
        gfx_fill_rect(TEXT_X, ROW_Y(vline) + LINE_H / 2, GFX_W - 2 * TEXT_X, 1, DIM);
    vline++;

    /* NEARBY */
    if (SHOWN(vline)) draw_header(ROW_Y(vline), "NEARBY");
    vline++;

    /* Scan action row */
    if (SHOWN(vline)) {
        int y = ROW_Y(vline);
        bool selected = (s_sel == (int)s_nknown);
        if (selected) gfx_draw_text(CARET_X, y, ">", ACCENT, 1);
        if (bluetooth_is_scanning()) gfx_draw_text(TEXT_X, y, "Scanning...", ACCENT, 1);
        else                         gfx_draw_text(TEXT_X, y, "Scan", GFX_WHITE, 1);
    }
    vline++;

    /* Nearby results */
    for (size_t i = 0; i < s_nnear; i++, vline++) {
        if (!SHOWN(vline)) continue;
        int y = ROW_Y(vline);
        bool selected = (s_sel == (int)s_nknown + 1 + (int)i);
        if (selected) gfx_draw_text(CARET_X, y, ">", ACCENT, 1);
        draw_name(TEXT_X, y, s_near[i].name, GFX_WHITE);
        if (is_connecting_dev(s_near[i].bda)) draw_connecting(y);
        else                                  draw_signal(y, s_near[i].rssi);
    }

    #undef ROW_Y
    #undef SHOWN

    /* Failure feedback on the bottom line: an action error (s_error) or a connect we issued that
       resolved to FAILED. Cleared by the next scan/connect. */
    const char *msg = s_error;
    if (!msg && s_connecting && bluetooth_get_conn_state() == BLUETOOTH_CONN_FAILED)
        msg = "Connection failed";
    if (msg) {
        int y = GFX_H - LINE_H;
        gfx_fill_rect(0, y - 2, GFX_W, LINE_H + 2, GFX_BLACK);
        gfx_draw_text(TEXT_X, y, msg, ACCENT, 1);
    }

    /* Live refresh only while there is something to watch; otherwise block (no wake-ups).
       Connected counts: the status dot must turn red on its own if the link drops. */
    self->refresh_ms = (bluetooth_is_scanning() ||
                        bluetooth_get_conn_state() == BLUETOOTH_CONN_CONNECTING ||
                        bluetooth_is_connected()) ? REFRESH_MS : 0;
}

/* Centered popup over the list: device name, then Connect/Disconnect and Forget. */
static void render_popup(screen_t *self)
{
    (void)self;
    render_list(self);   /* keep the list underneath; the box sits on top */

    const int bw = 140, bh = 62;
    const int bx = (GFX_W - bw) / 2, by = (GFX_H - bh) / 2;
    gfx_fill_rect(bx, by, bw, bh, GFX_BLACK);
    gfx_draw_rect(bx, by, bw, bh, GFX_WHITE);

    draw_name(bx + 6, by + 6, s_popup_dev.name, ACCENT);

    const char *o0 = is_connected_dev(s_popup_dev.bda) ? "Disconnect" : "Connect";
    const char *o1 = "Forget";
    int y0 = by + 26, y1 = by + 42;
    if (s_popup_sel == 0) gfx_draw_text(bx + 6, y0, ">", ACCENT, 1);
    else                  gfx_draw_text(bx + 6, y1, ">", ACCENT, 1);
    gfx_draw_text(bx + 16, y0, o0, GFX_WHITE, 1);
    gfx_draw_text(bx + 16, y1, o1, GFX_WHITE, 1);
}

/* Centered confirmation before a scan that would interrupt Bluetooth playback: OK / Cancel. */
static void render_confirm(screen_t *self)
{
    render_list(self);

    const int bw = 148, bh = 60;
    const int bx = (GFX_W - bw) / 2, by = (GFX_H - bh) / 2;
    gfx_fill_rect(bx, by, bw, bh, GFX_BLACK);
    gfx_draw_rect(bx, by, bw, bh, GFX_WHITE);

    gfx_draw_text(bx + 6, by + 6,  "Interrupt playback?",  ACCENT, 1);
    gfx_draw_text(bx + 6, by + 20, "Scan stops BT audio.", GFX_WHITE, 1);

    int ok_x = bx + 20, cancel_x = bx + 78, oy = by + 42;
    gfx_draw_text((s_confirm_sel == 0 ? ok_x : cancel_x) - 8, oy, ">", ACCENT, 1);
    gfx_draw_text(ok_x,     oy, "OK",     GFX_WHITE, 1);
    gfx_draw_text(cancel_x, oy, "Cancel", GFX_WHITE, 1);
}

static void render(screen_t *self)
{
    reload();
    s_blink = !s_blink;                                   /* advance the connecting-dot blink phase */
    if (s_sel >= sel_count()) s_sel = sel_count() - 1;   /* list shrank (e.g. after Forget) */
    if (s_sel < 0) s_sel = 0;
    if (s_scan_confirm) render_confirm(self);
    else if (s_popup)   render_popup(self);
    else                render_list(self);
}

/* ---- input ----------------------------------------------------------------------- */

static void activate(void)
{
    int idx; row_kind_t k = sel_kind(s_sel, &idx);
    switch (k) {
    case ROW_KNOWN:                        /* open the popup for this paired device */
        s_popup_dev = s_known[idx];
        s_popup_sel = 0;
        s_popup     = true;
        break;
    case ROW_SCAN:
        if (bluetooth_is_scanning()) {
            bluetooth_scan_stop();
        } else if (bt_audio_active()) {
            s_confirm_sel  = 0;              /* scan would cut the Bluetooth stream: confirm first */
            s_scan_confirm = true;
        } else {
            start_scan();
        }
        break;
    case ROW_NEAR:
        s_error = NULL;
        if (!power_on()) break;
        if (bluetooth_connect(s_near[idx].bda) != ESP_OK) { s_error = "Connect failed"; break; }
        memcpy(s_connecting_bda, s_near[idx].bda, ESP_BD_ADDR_LEN);
        s_connecting = true;
        break;
    }
}

static void popup_activate(void)
{
    if (s_popup_sel == 0) {                 /* Connect / Disconnect */
        if (is_connected_dev(s_popup_dev.bda)) {
            bluetooth_disconnect();
        } else {
            s_error = NULL;
            if (power_on()) {
                if (bluetooth_connect(s_popup_dev.bda) != ESP_OK) {
                    s_error = "Connect failed";
                } else {
                    memcpy(s_connecting_bda, s_popup_dev.bda, ESP_BD_ADDR_LEN);
                    s_connecting = true;
                }
            }
        }
    } else {                                /* Forget */
        bluetooth_forget_device(s_popup_dev.bda);
    }
    s_popup = false;
}

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;

    if (s_scan_confirm) {
        switch (ev) {
        case UI_EVENT_UP:
        case UI_EVENT_DOWN:
        case UI_EVENT_LEFT:
        case UI_EVENT_RIGHT:  s_confirm_sel ^= 1;                          break;
        case UI_EVENT_SELECT: if (s_confirm_sel == 0) start_scan();
                              s_scan_confirm = false;                      break;
        case UI_EVENT_BACK:   s_scan_confirm = false;                      break;
        default: break;
        }
        return;
    }

    if (s_popup) {
        switch (ev) {
        case UI_EVENT_UP:
        case UI_EVENT_DOWN:   s_popup_sel ^= 1;        break;
        case UI_EVENT_SELECT: popup_activate();        break;
        case UI_EVENT_BACK:   s_popup = false;         break;
        default: break;
        }
        return;
    }

    switch (ev) {
    case UI_EVENT_UP:     if (s_sel > 0)                 s_sel--; break;
    case UI_EVENT_DOWN:   if (s_sel < sel_count() - 1)   s_sel++; break;
    case UI_EVENT_SELECT: activate();                            break;
    case UI_EVENT_BACK:   navigator_pop();                       break;
    default: break;
    }
}

/* ---- lifecycle ------------------------------------------------------------------- */

static void bt_on_enter(screen_t *self)
{
    (void)self;
    bluetooth_load_known();                 /* paired list from NVS, radio stays off */
    /* This screen owns the radio while open: a manual disconnect or a failed connect must keep
       it up for the user's next action, so suppress the service's auto power-down until exit. */
    bluetooth_set_auto_shutdown(false);
    s_powered      = bluetooth_is_connected();   /* already up if a link is live */
    s_popup        = false;
    s_scan_confirm = false;
    s_connecting   = false;
    s_error        = NULL;
    s_sel     = 0;
    s_top     = 0;
}

static void bt_on_exit(screen_t *self)
{
    (void)self;
    /* Back to the service watching the link: once we leave, a drop (or a connect still in
       flight that fails) must power the radio down by itself. */
    bluetooth_set_auto_shutdown(true);
    if (bluetooth_is_scanning()) bluetooth_scan_stop();
    /* Power the radio down unless a link is up (bluetooth_shutdown refuses while connected). */
    if (!bluetooth_is_connected()) {
        bluetooth_shutdown();
        s_powered = false;
    }
}

screen_t *bluetooth_settings_screen(void)
{
    static screen_t s = {
        .on_enter     = bt_on_enter,
        .on_exit      = bt_on_exit,
        .handle_input = handle_input,
        .render       = render,
    };
    return &s;
}
