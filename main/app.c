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
    esp_pm_config_t pm = { .max_freq_mhz = 160, .min_freq_mhz = 80 };
    ESP_ERROR_CHECK(esp_pm_configure(&pm));

    /* Bring up NVS first: settings_init() runs nvs_flash_init() (erase-on-corrupt),
       so every NVS consumer (settings, bluetooth) can just nvs_open afterwards. */
    ESP_ERROR_CHECK(settings_init());

    /* Shared I2C bus + GPIO expander. The expander backs the buttons (read by the
       input service) and the DAC mute / amp shutdown, so it must be up before
       input_init() — otherwise its ISR reads an uninitialized I2C handle. */
    ESP_ERROR_CHECK(i2c_bus_init(&s_i2c));
    ESP_ERROR_CHECK(gpio_expander_init(s_i2c));

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
   (they live in the audio/maintenance tasks). Hardcoded for now; meant to become a
   user-adjustable setting once the settings menu exists. */
#define SCREEN_OFF_TIMEOUT_MS 30000

/* While locked, redraw the hint at a slightly shifted position on this period so no
   pixel ages faster than its neighbours (OLED burn-in). Each redraw is one frame
   render + SPI blit (~20 ms every 30 s): negligible energy. */
#define LOCK_SHIFT_MS 30000

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

static void ui_task(void *arg)
{
    (void)arg;

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
    xTaskCreate(storage_scan_task, "storage_scan", 4096, NULL, 1, NULL);

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
    unsigned lock_phase = 0;
    for (;;) {
        TickType_t wait = pdMS_TO_TICKS(LOCK_SHIFT_MS);   /* locked: next hint shift */
        if (screen_on) {
            uint32_t idle = input_idle_ms();
            if (idle >= SCREEN_OFF_TIMEOUT_MS) {
                lock_phase = 0;
                draw_lock_screen(lock_phase);
                screen_on = false;
            } else {
                TickType_t left = pdMS_TO_TICKS(SCREEN_OFF_TIMEOUT_MS - idle) + 1;
                wait = navigator_refresh_ticks();
                if (left < wait) wait = left;
            }
        }

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
