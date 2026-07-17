#include "app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "settings.h"
#include "spi_bus.h"
#include "i2c_bus.h"
#include "gpio_expander.h"
#include "display_oled.h"
#include "input.h"
#include "power.h"
#include "fuel_gauge.h"
#include "maintenance.h"
#include "autonomy.h"
#include "storage.h"
#include "playlist.h"
#include "audio_dac.h"
#include "sink_i2s_dac.h"
#include "volume.h"
#include "pipeline.h"
#include "player.h"
#include "navigator.h"
#include "status_bar.h"
#include "root_menu.h"
#include "screen_settings.h"
#include "gfx.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_task_wdt.h"
#include "diag.h"
#include <stdio.h>
#include <string.h>

/* Shared I2C master bus, owned here and handed to every device on it (expander now;
   fuel gauge / DAC as those services are wired in). */
static i2c_master_bus_handle_t s_i2c;

static void storage_scan_task(void *arg)
{
    (void)arg;
    esp_err_t err = storage_init();
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
        ESP_LOGW("app", "storage_init: %s", esp_err_to_name(err));
    }
    playlist_sync();

    /* Re-select the track that was current at the last auto power-off so Now Playing
       shows it again. Playback restarts at 0:00 — resuming mid-track would need seek
       support in the decoder (the saved "last_pos" is waiting for it). */
    char last[STORAGE_PATH_MAX];
    if (settings_get_str("last_path", last, sizeof last) == ESP_OK) {
        for (size_t i = 0, n = storage_count(); i < n; ++i) {
            const char *p = storage_track_path(i);
            if (p && strcmp(p, last) == 0) {
                playlist_select(i, NULL);
                break;
            }
        }
    }

    /* SD is mounted now: save any crash core dump to the card (then erase it from flash),
       export a pending autonomy run (from a previous battery-shutdown) to a CSV, and load
       the last-run summary for the battery-test screen. If no card was inserted, the dump
       stays in flash and the hot-plug mount (maintenance) exports it later. */
    power_coredump_export();
    autonomy_boot_export();

    diag_task_exit();
    vTaskDelete(NULL);
}

void app_init(void)
{
    /* Self-hold power before anything else: take over EnableReg from USB/the power
       button at boot, or releasing them cuts the rail. (power_self_hold does the
       glitch-free preload-high-then-drive; see power.c.) */
    power_self_hold();

    /* DFS: let the CPU drop to 80 MHz whenever nothing demands more. Drivers (I2S,
       SPI, I2C, BT) hold APB/CPU power-management locks while they are active, so
       playback is never starved. No light sleep: it would gate the I2S clocks, and
       the BT low-power clock would need a 32 kHz crystal this board doesn't have. */
    esp_pm_config_t pm = { .max_freq_mhz = 160, .min_freq_mhz = 160 };
    ESP_ERROR_CHECK(esp_pm_configure(&pm));

    /* Bring up NVS first: settings_init() runs nvs_flash_init() (erase-on-corrupt),
       so every NVS consumer (settings, bluetooth) can just nvs_open afterwards. */
    ESP_ERROR_CHECK(settings_init());

    /* Classify how the last session ended (clean / crash / power loss) before anything
       can power off and rewrite the marker. */
    power_boot_off_check();

    /* Shared I2C bus + GPIO expander. The expander backs the buttons (read by the
       input service) and the DAC mute / amp shutdown, so it must be up before
       input_init() — otherwise its ISR reads an uninitialized I2C handle. */
    ESP_ERROR_CHECK(i2c_bus_init(&s_i2c));
    ESP_ERROR_CHECK(gpio_expander_init(s_i2c));

    /* USB-C mux hand-off (TC7USB40MU): the charger gets the data lines first for its
       source detection, then a short-lived task hands them to the console once INOKB
       asserts. Must follow gpio_expander_init() -- the task reads INOKB off it. */
    power_usb_autoroute_start();

    /* Battery: the MAX17260 fuel gauge on the shared I2C bus, then the maintenance
       task that polls it through power_tick() every 2 s. The gauge and the expander
       (INOKB) must both be up first -- power_tick() reads both. This is what fills
       the status bar's battery percentage. */
    ESP_ERROR_CHECK(fuel_gauge_init(s_i2c));
    ESP_ERROR_CHECK(maintenance_init());

    /* Audio chain: PCM5242 I2C control, then I2S sink, then volume (ADC pot + DAC writes),
       then the pipeline (spawns audio task), then the player (wires end-of-track callback). */
    ESP_ERROR_CHECK(audio_dac_init(s_i2c));
    ESP_ERROR_CHECK(sink_i2s_dac_init());
    ESP_ERROR_CHECK(volume_init());
    ESP_ERROR_CHECK(pipeline_init());
    ESP_ERROR_CHECK(player_init());
}

/* UI task sizing: core 1 keeps the UI away from the BT controller + Bluedroid + SBC
   encoding that saturate core 0 while streaming; priority 5 stays below the audio task
   (22) so rendering never starves playback. 8 KiB covers the deepest UI path (screen
   render -> track_meta_read -> FATFS -> SD SPI) plus interrupt frames — the 3.5 KiB
   main-task stack this loop used to run on overflowed there. */
#define UI_TASK_STACK 8192
#define UI_TASK_PRIO  5
#define UI_TASK_CORE  1

/* Screen lock: after this long without a button press, the UI is replaced by a black
   frame with a dim hint text, so a locked device is distinguishable from a powered-off
   one. Only A (SELECT) unlocks — the waking press is swallowed — and every other button
   is ignored, so pocket presses do nothing. Playback and the volume pot are untouched
   (they live in the audio/maintenance tasks). The timeout is a user preference
   (power_get_sleep_ms(), Settings > Power); 0 disables the lock entirely. */

/* While locked, redraw the hint at a slightly shifted position on this period so no
   pixel ages faster than its neighbours (OLED burn-in). Each redraw is one frame
   render + SPI blit (~20 ms every 30 s): negligible energy. */
#define LOCK_SHIFT_MS 30000

/* Autonomy test screen: poll this often so an external abort (USB plugged, decided by the
   autonomy service) is caught within ~1 s, while the frame is only redrawn every LOCK_SHIFT_MS. */
#define TEST_POLL_MS 1000

static void draw_circle(int cx, int cy, int r, gfx_color_t color)
{
    int x = r, y = 0, err = 1 - r;   /* midpoint circle */
    while (x >= y) {
        gfx_pixel(cx + x, cy + y, color); gfx_pixel(cx - x, cy + y, color);
        gfx_pixel(cx + x, cy - y, color); gfx_pixel(cx - x, cy - y, color);
        gfx_pixel(cx + y, cy + x, color); gfx_pixel(cx - y, cy + x, color);
        gfx_pixel(cx + y, cy - x, color); gfx_pixel(cx - y, cy - x, color);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

/* Lock screen: two dim centered lines, the 'A' circled like the physical button.
   Black pixels draw no OLED current, so the whole frame costs the panel's quiescent
   draw plus ~200 dim pixels. phase cycles the anti-burn-in offsets. */
static void draw_lock_screen(unsigned phase)
{
    static const struct { int8_t dx, dy; } k_off[] = {
        {0, 0}, {3, 2}, {-3, -2}, {-3, 2}, {3, -2},
    };
    static const char l1[] = "Sleep mode";
    static const char l2[] = "Press A to wake up";
    const gfx_color_t gray = gfx_rgb(90, 90, 90);
    int dx = k_off[phase % (sizeof k_off / sizeof k_off[0])].dx;
    int dy = k_off[phase % (sizeof k_off / sizeof k_off[0])].dy;

    int y1 = (GFX_H - (2 * GFX_CHAR_H + 8)) / 2 + dy;   /* 8 px between the lines */
    int y2 = y1 + GFX_CHAR_H + 8;
    int x1 = (GFX_W - (int)(sizeof l1 - 1) * GFX_CHAR_W) / 2 + dx;
    int x2 = (GFX_W - (int)(sizeof l2 - 1) * GFX_CHAR_W) / 2 + dx;

    gfx_clear(GFX_BLACK);
    gfx_draw_text(x1, y1, l1, gray, 1);
    gfx_draw_text(x2, y2, l2, gray, 1);
    /* ring the 'A' (char 6 of l2): centered on its 5x7 glyph, radius clearing it
       into the blank space cells on either side */
    draw_circle(x2 + 6 * GFX_CHAR_W + 2, y2 + 3, 6, gray);
    gfx_flush();
}

/* Autonomy-test-in-progress screen: a screensaver sibling of the lock screen. Shows the
   workload and elapsed time in dim text, shifted by phase (same anti-burn-in scheme), with a
   "Press B to cancel" hint. Redrawn on the lock-shift period, so the duration ticks in minutes
   and no pixel ages faster than its neighbours; energy per redraw is negligible. */
static void draw_test_screen(const autonomy_status_t *st, unsigned phase)
{
    static const struct { int8_t dx, dy; } k_off[] = {
        {0, 0}, {3, 2}, {-3, -2}, {-3, 2}, {3, -2},
    };
    static const char *const k_mode[] = { "IDLE mode", "JACK mode", "BT mode" };
    const gfx_color_t gray = gfx_rgb(90, 90, 90);
    int dx = k_off[phase % (sizeof k_off / sizeof k_off[0])].dx;
    int dy = k_off[phase % (sizeof k_off / sizeof k_off[0])].dy;

    uint32_t s = st->elapsed_ms / 1000, h = s / 3600, m = (s % 3600) / 60;
    char dur[24];
    if (h > 0) snprintf(dur, sizeof dur, "Duration: %uh%02um", (unsigned)h, (unsigned)m);
    else       snprintf(dur, sizeof dur, "Duration: %um", (unsigned)m);

    /* Live battery: state of charge and the fuel gauge's time-to-empty (cached, no I2C here). */
    power_state_t ps;
    power_get_state(&ps);
    char batt[28];
    if (ps.valid) {
        uint32_t t = (uint32_t)ps.tte_s, th = t / 3600, tm = (t % 3600) / 60;
        snprintf(batt, sizeof batt, "%.1f %% - TTE %02u:%02u", ps.soc_pct, (unsigned)th, (unsigned)tm);
    } else {
        snprintf(batt, sizeof batt, "-- %% - TTE --:--");
    }

    const char *lines[] = { "Autonomy test in progress", k_mode[st->type], dur, batt, "Press B to cancel" };
    const int n = (int)(sizeof lines / sizeof lines[0]);
    const int step = GFX_CHAR_H + 6;
    int y0 = (GFX_H - (n * GFX_CHAR_H + (n - 1) * 6)) / 2 + dy;

    gfx_clear(GFX_BLACK);
    for (int i = 0; i < n; i++) {
        int w = (int)strlen(lines[i]) * GFX_CHAR_W;
        gfx_draw_text((GFX_W - w) / 2 + dx, y0 + i * step, lines[i], gray, 1);
    }
    gfx_flush();
}

/* Big error shown when a run is aborted mid-test (USB plugged). Dismissed by any key. */
static void draw_test_error(void)
{
    const gfx_color_t red = gfx_rgb(255, 40, 40);
    const char *l1 = "USB PLUGGED";
    const char *l2 = "Test aborted";
    const char *l3 = "Press any key";
    gfx_clear(GFX_BLACK);
    gfx_draw_text((GFX_W - (int)strlen(l1) * GFX_CHAR_W * 2) / 2, 54, l1, red, 2);
    gfx_draw_text((GFX_W - (int)strlen(l2) * GFX_CHAR_W * 2) / 2, 82, l2, red, 2);
    gfx_draw_text((GFX_W - (int)strlen(l3) * GFX_CHAR_W) / 2, 120, l3, gfx_rgb(120, 120, 120), 1);
    gfx_flush();
}

static void ui_task(void *arg)
{
    (void)arg;
    diag_register_task(UI_TASK_STACK);

    /* Bring up the panel (it owns its SPI device) and the button path from this task,
       so their interrupts are allocated on core 1 too, then hand the screen over to
       the navigator. */
    spi_device_handle_t disp_spi;
    ESP_ERROR_CHECK(spi_bus_display_init(&disp_spi));
    ESP_ERROR_CHECK(display_oled_init(disp_spi));
    screen_settings_restore();               /* apply the saved brightness before the first paint */
    ESP_ERROR_CHECK(input_init());

    /* SD scan in background so the UI is responsive while the card is enumerated.
       Priority 1 (just above idle) keeps it below the UI task. Stack covers the
       recursive scan (3 levels × 320 B frame) with margin. */
    xTaskCreate(storage_scan_task, "storage_scan", 5120, NULL, 1, NULL);

    /* Push the home screen and paint it once (navigator_push doesn't render). */
    screen_t *home = root_menu();
    navigator_push(home);
    home->render(home);
    status_bar_draw();
    gfx_flush();

    /* UI loop: wait for input (no busy-poll = idle CPU between presses), dispatch each
       event to the top screen, then present the frame it drew. A live screen (one with
       refresh_ms > 0, e.g. the stats pages) shortens the wait so it gets re-rendered on
       its timer; every other screen blocks indefinitely and costs no periodic wake-up.
       The wait is also capped at the screen-off deadline so the panel blanks on time. */
    bool screen_on = true;
    bool wdt_on = false;                                  /* UI task subscribed to the task WDT? */
    unsigned lock_phase = 0;
    for (;;) {
        /* Autonomy test in progress: own the screen like the lock does, bypassing the navigator
           and the idle-lock (the test screen IS the screensaver here). B cancels; other buttons
           are ignored. Poll in TEST_POLL_MS slices so an external abort (USB) is caught quickly,
           but only redraw every LOCK_SHIFT_MS so the panel cost stays negligible. */
        {
            static bool     test_prev;
            static unsigned test_phase;
            static int      test_poll;
            bool running = autonomy_is_running();

            if (test_prev && !running) {
                /* The run just ended (B, or a service-side abort). Show the error for an abort,
                   then return to the battery-test screen, which reflects the last result. */
                if (autonomy_get_last_result() == AUTONOMY_RESULT_ABORTED) {
                    draw_test_error();
                    ui_event_t ev;
                    input_get_event(&ev, portMAX_DELAY);   /* any key dismisses; also resets idle */
                }
                screen_on = true;
                navigator_render();
                gfx_flush();
            }
            if (running && !test_prev) { test_phase = 0; test_poll = 0; }
            test_prev = running;

            if (running) {
                if (wdt_on) { esp_task_wdt_delete(NULL); wdt_on = false; }
                if (test_poll == 0) {   /* redraw on entry and each shift period */
                    autonomy_status_t st;
                    autonomy_get_status(&st);
                    draw_test_screen(&st, test_phase);
                }
                ui_event_t ev;
                if (input_get_event(&ev, pdMS_TO_TICKS(TEST_POLL_MS))) {
                    if (ev == UI_EVENT_BACK) autonomy_cancel();
                    test_poll = 0;   /* force a redraw next pass if still running */
                } else if (++test_poll >= (int)(LOCK_SHIFT_MS / TEST_POLL_MS)) {
                    test_poll = 0;
                    test_phase++;    /* shift the text + tick the duration */
                }
                continue;
            }
        }

        TickType_t wait = pdMS_TO_TICKS(LOCK_SHIFT_MS);   /* locked: next hint shift */
        bool live = false;                                /* a live screen this iteration? */
        if (screen_on) {
            uint32_t sleep_ms = power_get_sleep_ms();     /* 0 = never sleep */
            uint32_t idle = input_idle_ms();
            if (sleep_ms != 0 && idle >= sleep_ms) {
                lock_phase = 0;
                draw_lock_screen(lock_phase);
                screen_on = false;
            } else {
                wait = navigator_refresh_ticks();
                live = wait != portMAX_DELAY;             /* re-renders within refresh_ms */
                if (sleep_ms != 0) {                       /* cap the wait at the sleep deadline */
                    TickType_t left = pdMS_TO_TICKS(sleep_ms - idle) + 1;
                    if (left < wait) wait = left;
                }
            }
        }

        /* Guard live screens only: they cycle every refresh_ms (<< the 5 s WDT timeout), so a
           stuck render trips the watchdog. Static and locked screens block up to 30 s by design,
           so they stay unsubscribed -- otherwise the wait itself would look like a softlock. */
        if (live && !wdt_on)  { esp_task_wdt_add(NULL);    wdt_on = true;  }
        if (!live && wdt_on)  { esp_task_wdt_delete(NULL); wdt_on = false; }
        if (wdt_on) esp_task_wdt_reset();

        ui_event_t ev;
        if (!input_get_event(&ev, wait)) {
            if (!screen_on) {
                draw_lock_screen(++lock_phase);   /* shift the hint (burn-in spread) */
                continue;
            }
            navigator_render();   /* timed out: refresh the live screen's data */
            gfx_flush();
            continue;
        }

        if (!screen_on) {
            if (ev != UI_EVENT_SELECT) continue;   /* pocket press: ignore */
            navigator_render();   /* content went stale while locked: redraw it */
            gfx_flush();
            screen_on = true;
            continue;             /* the waking press is swallowed, not dispatched */
        }

        navigator_tick(ev);
        gfx_flush();
    }
}

void app_run(void)
{
    /* Hand the UI over to its own task and return: app_main's exit deletes the main
       task, whose undersized stack the UI loop used to overflow. */
    xTaskCreatePinnedToCore(ui_task, "ui", UI_TASK_STACK, NULL, UI_TASK_PRIO, NULL,
                            UI_TASK_CORE);
}
