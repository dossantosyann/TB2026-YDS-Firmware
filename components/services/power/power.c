/**
 * @file power.c
 * @brief Power service implementation: fuel-gauge snapshot, charge/level policy, on/off.
 * @ingroup services_power
 */
#include "power.h"
#include "board_pins.h"
#include "fuel_gauge.h"
#include "gpio_expander.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "power";

/* Battery policy. SoC comes from the MAX17260 ModelGauge m5, so it is already
   cell-aware; the bands are plain percentages. */
#define LOW_SOC_PCT        15.0f
#define CRITICAL_SOC_PCT    5.0f
#define CHARGE_CURRENT_MA  10.0f   /* current into the cell above this = charging */

/* USB mux hand-off: charger detects the source, then the console takes the lines. */
#define USB_AUTOROUTE_TIMEOUT_MS  10000
#define USB_AUTOROUTE_POLL_MS       100

static power_state_t          s_state;          /* zero-init: valid=false until the first good read */
static power_shutdown_hook_t  s_shutdown_hook;  /* NULL until registered */

void power_self_hold(void)
{
    gpio_set_level(PIN_REG_EN, 1);
    gpio_set_direction(PIN_REG_EN, GPIO_MODE_OUTPUT);
}

void power_tick(void)
{
    fuel_gauge_data_t d;
    if (fuel_gauge_read(&d) != ESP_OK) {
        s_state.valid = false;   /* never act on a bad read */
        return;
    }

    /* INOKB on expander IO7. Active-low (verified on hardware: bit7 low at rest with
       USB connected), so low = input power present. */
    bool inokb = true;   /* default to "no external power" if the read fails */
    gpio_expander_get(EXP_INOKB, &inokb);

    s_state.valid          = true;
    s_state.soc_pct        = d.soc_pct;
    s_state.voltage_v      = d.voltage_v;
    s_state.current_ma     = d.current_ma;
    s_state.charging       = d.current_ma > CHARGE_CURRENT_MA;
    s_state.external_power  = !inokb;

    if (d.soc_pct <= CRITICAL_SOC_PCT)   s_state.level = POWER_LEVEL_CRITICAL;
    else if (d.soc_pct <= LOW_SOC_PCT)   s_state.level = POWER_LEVEL_LOW;
    else                                 s_state.level = POWER_LEVEL_NORMAL;

    /* Graceful auto-off: only on a real battery that is critically low and not being
       fed. The external-power gate keeps the device alive on USB / the bench. */
    if (s_state.level == POWER_LEVEL_CRITICAL && !s_state.charging && !s_state.external_power) {
        ESP_LOGW(TAG, "battery critical (%.0f%%), shutting down", d.soc_pct);
        power_shutdown();
    }
}

void power_get_state(power_state_t *out)
{
    *out = s_state;
}

void power_set_usb_route(power_usb_route_t route)
{
    /* PIN_USB_DIR drives the TC7USB40MU select: HIGH = MAX77757 charger, LOW = CP2102N. */
    gpio_set_level(PIN_USB_DIR, route == POWER_USB_CHARGE ? 1 : 0);
    gpio_set_direction(PIN_USB_DIR, GPIO_MODE_OUTPUT);
}

static void usb_autoroute_task(void *arg)
{
    (void)arg;
    /* Wait for INOKB to assert (low = input detected) or the safety timeout, then hand
       the data lines to the console. Poll the expander; the charger does its detection
       on the lines we routed to it in power_usb_autoroute_start(). */
    for (uint32_t elapsed = 0; elapsed < USB_AUTOROUTE_TIMEOUT_MS; elapsed += USB_AUTOROUTE_POLL_MS) {
        bool inokb = true;
        if (gpio_expander_get(EXP_INOKB, &inokb) == ESP_OK && !inokb) break;
        vTaskDelay(pdMS_TO_TICKS(USB_AUTOROUTE_POLL_MS));
    }
    power_set_usb_route(POWER_USB_DATA);
    ESP_LOGI(TAG, "USB lines routed to console (CP2102N)");
    vTaskDelete(NULL);
}

void power_usb_autoroute_start(void)
{
    power_set_usb_route(POWER_USB_CHARGE);   /* charger first: let it detect the source */
    xTaskCreate(usb_autoroute_task, "usbroute", 3072, NULL, 4, NULL);
}

void power_set_shutdown_hook(power_shutdown_hook_t hook)
{
    s_shutdown_hook = hook;
}

void power_shutdown(void)
{
    ESP_LOGW(TAG, "shutdown: releasing EnableReg");
    if (s_shutdown_hook) s_shutdown_hook();   /* e.g. amp off -> DAC off, in the audio service's order */
    gpio_set_level(PIN_REG_EN, 0);
    /* Rail collapses within the regulator's turn-off time. If USB still latches it
       on, power stays up; nothing more we can do. */
    for (;;) vTaskDelay(portMAX_DELAY);
}
