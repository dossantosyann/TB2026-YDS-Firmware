#include "app.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "board_pins.h"
#include "settings.h"

void app_init(void)
{
    /* Self-hold power: drive EnableReg (GPIO4) HIGH before anything else.
       The regulator is only latched on by USB/the power button at boot; if the
       ESP32 doesn't take over the hold immediately, releasing them cuts power.
       Preload the output register high, THEN enable the driver, so the pin never
       drives low (even briefly) on its way up. */
    gpio_set_level(PIN_REG_EN, 1);
    gpio_set_direction(PIN_REG_EN, GPIO_MODE_OUTPUT);

    /* Bring up NVS first: settings_init() runs nvs_flash_init() (erase-on-corrupt),
       so every NVS consumer (settings, bluetooth) can just nvs_open afterwards. */
    ESP_ERROR_CHECK(settings_init());
}

void app_run(void)
{
}
