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
#include "navigator.h"
#include "status_bar.h"
#include "root_menu.h"
#include "gfx.h"

/* Shared I2C master bus, owned here and handed to every device on it (expander now;
   fuel gauge / DAC as those services are wired in). */
static i2c_master_bus_handle_t s_i2c;

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
}

void app_run(void)
{
    /* Bring up the panel (it owns its SPI device) and the button path, then hand the
       screen over to the navigator. */
    spi_device_handle_t disp_spi;
    ESP_ERROR_CHECK(spi_bus_display_init(&disp_spi));
    ESP_ERROR_CHECK(display_oled_init(disp_spi));
    ESP_ERROR_CHECK(input_init());

    /* Push the home screen and paint it once (navigator_push doesn't render). */
    screen_t *home = root_menu();
    navigator_push(home);
    home->render(home);
    status_bar_draw();
    gfx_flush();

    /* UI task: block on the input queue (no busy-poll = idle CPU between presses),
       dispatch each event to the top screen, then present the frame it drew. */
    for (;;) {
        ui_event_t ev;
        if (input_get_event(&ev, portMAX_DELAY)) {
            navigator_tick(ev);
            gfx_flush();
        }
    }
}
