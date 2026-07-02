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
    vTaskDelete(NULL);
}

void app_init(void)
{
    /* Self-hold power before anything else: take over EnableReg from USB/the power
       button at boot, or releasing them cuts the rail. (power_self_hold does the
       glitch-free preload-high-then-drive; see power.c.) */
    power_self_hold();

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

void app_run(void)
{
    /* Bring up the panel (it owns its SPI device) and the button path, then hand the
       screen over to the navigator. */
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

    /* UI task: wait for input (no busy-poll = idle CPU between presses), dispatch each
       event to the top screen, then present the frame it drew. A live screen (one with
       refresh_ms > 0, e.g. the stats pages) shortens the wait so it gets re-rendered on
       its timer; every other screen blocks indefinitely and costs no periodic wake-up. */
    for (;;) {
        ui_event_t ev;
        if (input_get_event(&ev, navigator_refresh_ticks())) {
            navigator_tick(ev);
        } else {
            navigator_render();   /* timed out: refresh the live screen's data */
        }
        gfx_flush();
    }
}
