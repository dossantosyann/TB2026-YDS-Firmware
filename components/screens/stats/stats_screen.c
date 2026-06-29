/**
 * @file stats_screen.c
 * @brief Statistics / diagnostics menu: a top-level list over five live detail pages.
 *
 * The menu reuses no generic widget (it offsets content below the status bar, which the
 * generic menu_screen does not). Each detail page sets screen_t::refresh_ms so the UI
 * task re-renders it every DETAIL_REFRESH_MS for live values; the menu itself stays event-driven.
 */
#include "stats_screen.h"
#include "navigator.h"
#include "status_bar.h"
#include "gfx.h"

#include "power.h"
#include "fuel_gauge.h"
#include "storage.h"
#include "sdcard.h"
#include "bluetooth.h"
#include "input.h"
#include "volume.h"
#include "audio_dac.h"
#include "i2c_bus.h"
#include "driver/i2c_master.h"

#include "esp_timer.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_app_desc.h"
#include "esp_idf_version.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

/* ---- shared layout + line cursor ------------------------------------------------- */

#define PAD_X     6
#define TITLE_Y   (STATUS_BAR_H + 2)
#define BODY_Y    (TITLE_Y + 16)   /* gap under the title */
#define LINE_H    12

/* Peripherals page: bus groups are spaced by GROUP_GAP so a header sits the same
   distance below the previous line as BODY_Y sits below the title. STATUS_X is the
   shared column where every OK/FAIL/state token starts, so they line up. */
#define GROUP_GAP 4
#define STATUS_X  120

#define DETAIL_REFRESH_MS 250      /* live re-render cadence for the detail pages */

static int s_y;   /* current draw row; advanced by emit()/emitf() */

static void page_begin(const char *title)
{
    gfx_clear(GFX_BLACK);
    gfx_draw_text(PAD_X, TITLE_Y, title, GFX_WHITE, 1);
    s_y = BODY_Y;
}

static void emit(const char *s, gfx_color_t color)
{
    gfx_draw_text(PAD_X, s_y, s, color, 1);
    s_y += LINE_H;
}

static void emitf(gfx_color_t color, const char *fmt, ...)
{
    char buf[40];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    emit(buf, color);
}

/* Human-readable byte size into @p buf, e.g. "29.7 GB". */
static const char *fmt_size(char *buf, size_t n, uint64_t b)
{
    if (b >= (1ULL << 30)) snprintf(buf, n, "%.2f GB", b / (double)(1ULL << 30));
    else if (b >= (1ULL << 20)) snprintf(buf, n, "%.1f MB", b / (double)(1ULL << 20));
    else if (b >= (1ULL << 10)) snprintf(buf, n, "%.1f KB", b / (double)(1ULL << 10));
    else snprintf(buf, n, "%llu B", (unsigned long long)b);
    return buf;
}

/* Seconds into "1h23m" / "12m" / "--" (for invalid TTE/TTF the gauge reports huge). */
static const char *fmt_dur(char *buf, size_t n, float s)
{
    if (s <= 0.0f || s > 1.0e7f) { snprintf(buf, n, "--"); return buf; }
    int t = (int)s, h = t / 3600, m = (t % 3600) / 60;
    if (h > 0) snprintf(buf, n, "%dh%02dm", h, m);
    else snprintf(buf, n, "%dm", m);
    return buf;
}

/* ---- detail pages --------------------------------------------------------------- */

static void render_battery(screen_t *self)
{
    (void)self;
    page_begin("Battery");

    fuel_gauge_data_t d;
    if (fuel_gauge_read(&d) != ESP_OK) {
        emit("gauge read error", gfx_rgb(255, 80, 80));
        return;
    }
    char b[16];
    emitf(GFX_WHITE, "Charge:      %.1f %%", d.soc_pct);
    emitf(GFX_WHITE, "Voltage:     %.3f V", d.voltage_v);
    emitf(GFX_WHITE, "Current:     %.1f mA", d.current_ma);
    emitf(GFX_WHITE, "Capacity:    %.0f mAh", d.rep_cap_mah);
    if (d.current_ma >= 0.0f) emitf(GFX_WHITE, "Full in:     %s", fmt_dur(b, sizeof b, d.ttf_s));
    else                      emitf(GFX_WHITE, "Empty in:    %s", fmt_dur(b, sizeof b, d.tte_s));
    emitf(GFX_WHITE, "Cycles:      %.1f", d.cycles);
    emitf(GFX_WHITE, "Health:      %.0f %%", d.age_pct);
    emitf(GFX_WHITE, "Temperature: %.1f C", d.die_temp_c);

    power_state_t p;
    power_get_state(&p);
    emitf(GFX_WHITE, "Charging:    %s", p.charging ? "yes" : "no");
    emitf(GFX_WHITE, "External:    %s", p.external_power ? "yes" : "no");
}

static void render_storage(screen_t *self)
{
    (void)self;
    page_begin("Storage");

    emitf(GFX_WHITE, "Card:    %s", sdcard_present() ? "present" : "absent");
    emitf(GFX_WHITE, "Mounted: %s", storage_ready() ? "yes" : "no");

    uint64_t tot = 0, used = 0;
    if (storage_get_usage(&tot, &used) == ESP_OK) {
        char b[16];
        emitf(GFX_WHITE, "Total:   %s", fmt_size(b, sizeof b, tot));
        emitf(GFX_WHITE, "Used:    %s", fmt_size(b, sizeof b, used));
        emitf(GFX_WHITE, "Free:    %s", fmt_size(b, sizeof b, tot - used));
        emitf(GFX_WHITE, "Usage:   %d %%", tot ? (int)(used * 100 / tot) : 0);
        emitf(GFX_WHITE, "Tracks:  %u", (unsigned)storage_count());
    } else {
        emit("Total:   NA", GFX_WHITE);
        emit("Used:    NA", GFX_WHITE);
        emit("Tracks:  NA", GFX_WHITE);
    }
}

static const char *reset_reason_str(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_SW:        return "software";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "int wdt";
    case ESP_RST_TASK_WDT:  return "task wdt";
    case ESP_RST_WDT:       return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    default:                return "other";
    }
}

static void render_system(screen_t *self)
{
    (void)self;
    page_begin("System");

    uint32_t sec = (uint32_t)(esp_timer_get_time() / 1000000);
    uint32_t h = sec / 3600, m = (sec % 3600) / 60, s = sec % 60;
    emitf(GFX_WHITE, "Uptime:   %luh%02lum%02lus",
          (unsigned long)h, (unsigned long)m, (unsigned long)s);
    emitf(GFX_WHITE, "Heap:     %u KB", (unsigned)(esp_get_free_heap_size() / 1024));
    emitf(GFX_WHITE, "Min heap: %u KB", (unsigned)(esp_get_minimum_free_heap_size() / 1024));
    emitf(GFX_WHITE, "PSRAM:    %u KB",
          (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    emitf(GFX_WHITE, "Firmware: %s", esp_app_get_description()->version);
    emitf(GFX_WHITE, "IDF:      %s", esp_get_idf_version());
    emitf(GFX_WHITE, "Reset:    %s", reset_reason_str(esp_reset_reason()));
}

/* I2C device addresses on the shared bus (hardcoded in their drivers; no central table). */
#define ADDR_FUEL_GAUGE 0x36
#define ADDR_EXPANDER   0x38
#define ADDR_DAC        0x4C

/* Bus group header: a small gap (matching the title gap), then the bus name in white. */
static void group(const char *name)
{
    s_y += GROUP_GAP;
    emit(name, GFX_WHITE);
}

/* Sub-item: white label at PAD_X, an OK/FAIL token in colour at the aligned STATUS_X. */
static void check_line(const char *label, bool ok)
{
    gfx_draw_text(PAD_X, s_y, label, GFX_WHITE, 1);
    gfx_draw_text(STATUS_X, s_y, ok ? "OK" : "FAIL",
                  ok ? gfx_rgb(0, 255, 0) : gfx_rgb(255, 80, 80), 1);
    s_y += LINE_H;
}

/* Sub-item with a free-form state (kept white -- only OK/FAIL get colour, by request). */
static void state_line(const char *label, const char *state)
{
    gfx_draw_text(PAD_X, s_y, label, GFX_WHITE, 1);
    gfx_draw_text(STATUS_X, s_y, state, GFX_WHITE, 1);
    s_y += LINE_H;
}

static void probe_line(const char *name, i2c_master_bus_handle_t bus, uint8_t addr)
{
    char label[20];
    snprintf(label, sizeof label, "  %-10s 0x%02X", name, addr);
    check_line(label, bus && i2c_master_probe(bus, addr, 50) == ESP_OK);
}

/* PCM5242 clock-status flags we care about: D5 PLL unlocked, D1 BCK invalid, D0 fS invalid.
   D2 (SCK invalid) is ignored -- SCK is grounded on this board, so that bit is always set. */
#define DAC_CLK_BAD_MASK 0x23

static void render_peripherals(screen_t *self)
{
    (void)self;
    page_begin("Peripherals");

    /* I2C: the one bus we can actually probe by address. The title gap (BODY_Y) already
       separates this first header, so it is emitted without an extra GROUP_GAP. */
    i2c_master_bus_handle_t bus = i2c_bus_handle();
    emit("I2C", GFX_WHITE);
    probe_line("Fuel gauge", bus, ADDR_FUEL_GAUGE);
    probe_line("Expander",   bus, ADDR_EXPANDER);
    probe_line("DAC",        bus, ADDR_DAC);

    /* I2S is write-only (no ACK): the only real check is the DAC's clock-detect register,
       valid only while audio is streaming. OK when the PLL/BCK/fS are locked, else idle. */
    group("I2S");
    bool i2s_ok = false;
    if (audio_dac_ready()) {
        uint8_t cs;
        i2s_ok = audio_dac_get_clock_status(&cs) == ESP_OK && (cs & DAC_CLK_BAD_MASK) == 0;
    }
    if (i2s_ok) check_line("  DAC clock", true);
    else        state_line("  DAC clock", "idle");

    /* SPI is write-only too. The display is proven OK by the fact this page is visible. */
    group("SPI");
    check_line("  Display", true);
    state_line("  microSD",
               sdcard_present() ? (storage_ready() ? "mounted" : "present") : "absent");

    group("Bluetooth");
    state_line("  Status", bluetooth_is_connected() ? "connected" : "idle");
}

/* Button order must match BTN[] in input_logic.c (Up, Down, Left, Right, B, A). */
static const char *const s_btn_labels[INPUT_BTN_COUNT] = {
    "Up", "Down", "Left", "Right", "B (back)", "A (select)",
};

static void render_inputs(screen_t *self)
{
    (void)self;
    page_begin("Inputs");

    input_diag_t in;
    input_get_diag(&in);
    /* Column layout: label padded to width of "Potentiometer:" (14) so the ON/OFF
       state starts right after it, and the press count lands in the same column as
       the pot voltage below (both at column 20). */
    for (int i = 0; i < INPUT_BTN_COUNT; i++) {
        bool p = in.pressed[i];
        emitf(p ? gfx_rgb(0, 255, 0) : GFX_WHITE,
              "%-14s %-3s  %lu", s_btn_labels[i], p ? "ON" : "OFF",
              (unsigned long)in.count[i]);
    }

    s_y += GROUP_GAP;

    int mv = 0;
    if (volume_read_mv(&mv)) emitf(GFX_WHITE, "%-20s%d mV", "Potentiometer:", mv);
    else                     emit("Potentiometer:      n/a", gfx_rgb(255, 80, 80));
}

static void detail_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    if (ev == UI_EVENT_BACK) navigator_pop();
}

static void noop(screen_t *self) { (void)self; }

#define DETAIL(render_fn) {                  \
        .on_enter     = noop,                \
        .on_exit      = noop,                \
        .handle_input = detail_input,        \
        .render       = (render_fn),         \
        .refresh_ms   = DETAIL_REFRESH_MS,   \
    }

static screen_t s_battery = DETAIL(render_battery);
static screen_t s_storage = DETAIL(render_storage);
static screen_t s_system  = DETAIL(render_system);
static screen_t s_inputs  = DETAIL(render_inputs);
static screen_t s_peripherals = DETAIL(render_peripherals);

/* ---- top-level menu ------------------------------------------------------------- */

#define N_ITEMS    5
#define MENU_ROW_H 28
#define MENU_Y0    (STATUS_BAR_H + 6)
#define MENU_ARROW_X 4
#define MENU_TEXT_X  22
#define MENU_SCALE   2

static const char *const s_labels[N_ITEMS] = { "Battery", "Storage", "Inputs", "Peripherals", "System" };
static screen_t *const   s_targets[N_ITEMS] = { &s_battery, &s_storage, &s_inputs, &s_peripherals, &s_system };
static int s_sel = 0;

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

/* Sober selection: a single ">" caret on the active row (no full white bar, which on this
   passive OLED lights OFF pixels across the line; see root_menu's note). */
static void menu_render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);
    for (int i = 0; i < N_ITEMS; i++) {
        int y = MENU_Y0 + i * MENU_ROW_H;
        if (i == s_sel)
            gfx_draw_text(MENU_ARROW_X, y, ">", gfx_rgb(255, 120, 0), MENU_SCALE);
        gfx_draw_text(MENU_TEXT_X, y, s_labels[i], GFX_WHITE, MENU_SCALE);
    }
}

static screen_t s_menu = {
    .on_enter     = noop,
    .on_exit      = noop,
    .handle_input = menu_input,
    .render       = menu_render,
};

screen_t *stats_screen(void)
{
    return &s_menu;
}
