/**
 * @file power.c
 * @brief Power service implementation: fuel-gauge snapshot, charge/level policy, on/off.
 * @ingroup services_power
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "power.h"
#include "board_pins.h"
#include "fuel_gauge.h"
#include "gpio_expander.h"
#include "settings.h"
#include "driver/gpio.h"
#include "diag.h"
#include "nvs.h"
#include "esp_core_dump.h"
#include "esp_partition.h"
#include "storage.h"
#include "sdcard.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "power";

/* Battery policy. SoC comes from the MAX17260 ModelGauge m5, so it is already
   cell-aware; the bands are plain percentages. */
#define LOW_SOC_PCT        15.0f
#define CRITICAL_SOC_PCT    5.0f   /* red flag in the UI; the user's advance warning */
#define SHUTDOWN_SOC_PCT    1.0f   /* graceful auto-off, decoupled so the red flag warns first */
#define CHARGE_CURRENT_MA  10.0f   /* current into the cell above this = charging */

/* USB mux hand-off: charger detects the source, then the console takes the lines. This is the
   MAX77757 reference topology (datasheet "D+/D- Multiplexing"): the charger owns D+/D- long
   enough to run BC1.2, signals a valid input on INOKB, and only then does the console get them.
   The hold covers the worst-case detection: 900 ms DCD timeout + 39 ms primary-to-secondary +
   55 ms debounce. Handing the lines back sooner aborts BC1.2, and the input current limit stays
   at its 500 mA default instead of the 1.5 A a DCP/CDP can give. */
#define USB_AUTOROUTE_TIMEOUT_MS   5000   /* give up waiting for INOKB (no valid source) */
#define USB_CHARGER_HOLD_MS        1200
#define USB_AUTOROUTE_POLL_MS       100

/* Clean-shutdown marker. Releasing EnableReg collapses the rail, so the next boot always
   reads ESP_RST_POWERON whether the board crashed, lost power, or shut down on purpose.
   The only way to tell an intentional off apart afterwards is a flag persisted before
   the rail drops; it is consumed (read + erased) once at boot. A crash leaves a core
   dump in flash instead (written by the panic handler before the reset drops the rail),
   giving the three-way verdict clean / crash / power-loss. */
#define OFF_NVS_NS   "power"
#define OFF_NVS_KEY  "clean_off"

static power_off_cause_t s_off_cause;   /* zero-init = POWER_OFF_POWER_LOSS */
static char              s_crash_task[16];
static uint32_t          s_crash_pc;

static power_state_t          s_state;          /* zero-init: valid=false until the first good read */
static power_shutdown_hook_t  s_shutdown_hook;  /* NULL until registered */
static bool                   s_low_batt_shutdown = true;  /* built-in critical auto-off (autonomy suspends it) */

/* Autoroute re-arming. s_usb_routing keeps a second task from racing one already in flight.
   s_ext_prev starts true so a cable already in at boot does not re-trigger the hand-off that
   app_init() just ran; only a plug event (no external power -> external power) re-arms it. */
static volatile bool s_usb_routing;
static bool          s_ext_prev = true;

/* Last route driven onto PIN_USB_DIR, for the diagnostics page. Reads POWER_USB_DATA before the
   first power_set_usb_route() call, when the pin is not driven yet -- boot calls it right away. */
static power_usb_route_t s_usb_route;

/* Power-saving timeouts (user preferences, persisted in NVS via the settings service).
   Both are cached lazily on first read so the hot paths that consult them every loop
   (the UI task, the maintenance idle watch) never touch flash. 0 means "disabled". */
#define SLEEP_MS_KEY        "sleep_ms"
#define POWEROFF_MS_KEY     "poweroff_ms"
#define SLEEP_MS_DEFAULT    30000u          /* screen off after 30 s idle */
#define POWEROFF_MS_DEFAULT 300000u         /* auto power-off after 5 min idle */

static uint32_t s_sleep_ms;                 /* UINT32_MAX until loaded from NVS */
static uint32_t s_poweroff_ms;
static bool     s_sleep_loaded;
static bool     s_poweroff_loaded;

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

    /* INOKB on expander IO7: MAX77757 open-drain flag, low = valid input at CHGIN. A failed I2C
       read tells us nothing, so keep the previous verdict rather than reporting "unplugged" --
       a phantom unplug/replug pair would re-arm the autoroute and drop the console for nothing. */
    bool inokb;
    if (gpio_expander_get(EXP_INOKB, &inokb) == ESP_OK) s_state.external_power = !inokb;

    s_state.valid          = true;
    s_state.soc_pct        = d.soc_pct;
    s_state.voltage_v      = d.voltage_v;
    s_state.current_ma     = d.current_ma;
    s_state.tte_s          = d.tte_s;
    s_state.charging       = d.current_ma > CHARGE_CURRENT_MA;

    /* Plug event: give the charger the data lines back so it can redo BC1.2 on this source.
       Without it a hot-plugged legacy (USB-A) charger stays capped at the 500 mA default. */
    if (s_state.external_power && !s_ext_prev) power_usb_autoroute_start();
    s_ext_prev = s_state.external_power;

    if (d.soc_pct <= CRITICAL_SOC_PCT)   s_state.level = POWER_LEVEL_CRITICAL;
    else if (d.soc_pct <= LOW_SOC_PCT)   s_state.level = POWER_LEVEL_LOW;
    else                                 s_state.level = POWER_LEVEL_NORMAL;

    /* Graceful auto-off: only on a real battery that is near-empty (SoC < SHUTDOWN_SOC_PCT,
       i.e. the cell has reached ~VEmpty) and not being fed. The red CRITICAL flag warns the
       user earlier; the device only powers off here. The external-power gate keeps it alive
       on USB / the bench. Suspended while an autonomy test runs, which owns the shutdown so it
       can write its log first. */
    if (s_low_batt_shutdown &&
        power_soc_at_shutdown(d.soc_pct) && !s_state.charging && !s_state.external_power) {
        ESP_LOGW(TAG, "battery near-empty (%.0f%%), shutting down", d.soc_pct);
        power_shutdown();
    }
}

bool power_soc_at_shutdown(float soc_pct)
{
    return soc_pct <= SHUTDOWN_SOC_PCT;
}

void power_get_state(power_state_t *out)
{
    *out = s_state;
}

void power_set_usb_route(power_usb_route_t route)
{
    /* PIN_USB_DIR drives the PI3USB221AZUAEX select: HIGH = MAX77757 charger, LOW = CP2102N. */
    gpio_set_level(PIN_USB_DIR, route == POWER_USB_CHARGE ? 1 : 0);
    gpio_set_direction(PIN_USB_DIR, GPIO_MODE_OUTPUT);
    s_usb_route = route;
}

power_usb_route_t power_get_usb_route(void)
{
    return s_usb_route;
}

static void usb_autoroute_task(void *arg)
{
    (void)arg;
    /* Wait for INOKB to assert (valid input), then keep the lines on the charger for the hold so
       BC1.2 runs to completion. The hold is counted from the assertion, not from the start of the
       hand-off: a cable plugged in late in the wait window still gets its full detection time.
       If INOKB never asserts there is no source worth waiting for -- bail out at the timeout. The
       console gets its lines back on every path. */
    bool valid = false;
    for (uint32_t waited = 0; waited < USB_AUTOROUTE_TIMEOUT_MS; waited += USB_AUTOROUTE_POLL_MS) {
        bool inokb;
        if (gpio_expander_get(EXP_INOKB, &inokb) == ESP_OK && !inokb) { valid = true; break; }
        vTaskDelay(pdMS_TO_TICKS(USB_AUTOROUTE_POLL_MS));
    }
    if (valid) vTaskDelay(pdMS_TO_TICKS(USB_CHARGER_HOLD_MS));

    power_set_usb_route(POWER_USB_DATA);
    ESP_LOGI(TAG, "USB lines routed to console (CP2102N), source %s",
             valid ? "detected" : "absent");
    s_usb_routing = false;
    diag_task_exit();
    vTaskDelete(NULL);
}

void power_usb_autoroute_start(void)
{
    if (s_usb_routing) return;   /* one hand-off at a time: a replug during it changes nothing */
    s_usb_routing = true;

    power_set_usb_route(POWER_USB_CHARGE);   /* charger first: let it detect the source */
    if (xTaskCreate(usb_autoroute_task, "usbroute", 3072, NULL, 4, NULL) != pdPASS) {
        ESP_LOGW(TAG, "usbroute task failed; leaving the USB lines on the console");
        power_set_usb_route(POWER_USB_DATA);   /* never strand the lines on the charger */
        s_usb_routing = false;
    }
}

void power_set_low_batt_shutdown(bool enable)
{
    s_low_batt_shutdown = enable;
}

void power_set_shutdown_hook(power_shutdown_hook_t hook)
{
    s_shutdown_hook = hook;
}

uint32_t power_get_sleep_ms(void)
{
    if (!s_sleep_loaded) {
        if (settings_get_u32(SLEEP_MS_KEY, &s_sleep_ms) != ESP_OK) s_sleep_ms = SLEEP_MS_DEFAULT;
        s_sleep_loaded = true;
    }
    return s_sleep_ms;
}

void power_set_sleep_ms(uint32_t ms)
{
    s_sleep_ms = ms;
    s_sleep_loaded = true;
    settings_set_u32(SLEEP_MS_KEY, ms);
}

uint32_t power_get_poweroff_ms(void)
{
    if (!s_poweroff_loaded) {
        if (settings_get_u32(POWEROFF_MS_KEY, &s_poweroff_ms) != ESP_OK) s_poweroff_ms = POWEROFF_MS_DEFAULT;
        s_poweroff_loaded = true;
    }
    return s_poweroff_ms;
}

void power_set_poweroff_ms(uint32_t ms)
{
    s_poweroff_ms = ms;
    s_poweroff_loaded = true;
    settings_set_u32(POWEROFF_MS_KEY, ms);
}

void power_boot_off_check(void)
{
    bool clean = false;
    nvs_handle_t h;
    if (nvs_open(OFF_NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        uint8_t v = 0;
        if (nvs_get_u8(h, OFF_NVS_KEY, &v) == ESP_OK && v) {
            clean = true;
            nvs_erase_key(h, OFF_NVS_KEY);   /* consume it: a stale flag would mask the next crash */
            nvs_commit(h);
        }
        nvs_close(h);
    }

    /* A valid core dump in flash outlives the rail collapse, so it is the one reliable
       crash witness. It cannot coexist with a fresh marker (a crash never reaches
       power_shutdown()), so precedence is moot; check the marker first anyway. */
    if (clean) {
        s_off_cause = POWER_OFF_CLEAN;
    } else if (esp_core_dump_image_check() == ESP_OK) {
        s_off_cause = POWER_OFF_CRASH;
        esp_core_dump_summary_t sum;
        if (esp_core_dump_get_summary(&sum) == ESP_OK) {
            strlcpy(s_crash_task, sum.exc_task, sizeof s_crash_task);
            s_crash_pc = sum.exc_pc;
        }
        ESP_LOGW(TAG, "previous session crashed: task %s, PC 0x%08lx",
                 s_crash_task, (unsigned long)s_crash_pc);
    }
}

power_off_cause_t power_last_off_cause(void)
{
    return s_off_cause;
}

void power_coredump_export(void)
{
    size_t addr = 0, size = 0;
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
    if (!part || esp_core_dump_image_get(&addr, &size) != ESP_OK) {
        return;                       /* no dump pending */
    }
    if (!storage_ready()) {
        return;                       /* no SD: keep the dump for the next mount */
    }

    char path[48];
    FILE *f = NULL;
    for (int i = 0; i < 1000 && !f; i++) {        /* first unused name */
        snprintf(path, sizeof path, "%s/core_%03d.bin", SDCARD_MOUNT_POINT, i);
        f = fopen(path, "r");
        if (f) { fclose(f); f = NULL; continue; } /* taken: try the next */
        f = fopen(path, "w");
        break;
    }
    if (f) {
        char buf[512];
        size_t done = 0;
        bool ok = true;
        while (ok && done < size) {
            size_t n = size - done > sizeof buf ? sizeof buf : size - done;
            ok = esp_partition_read(part, done, buf, n) == ESP_OK &&
                 fwrite(buf, 1, n, f) == n;
            done += n;
        }
        fclose(f);
        if (ok) ESP_LOGW(TAG, "core dump saved to %s (%u bytes)", path, (unsigned)size);
        else    ESP_LOGE(TAG, "core dump copy to %s failed", path);
    }
    /* Erase even if the copy failed — a correct verdict next boot beats keeping an
       unreadable dump; a stale dump would label every later power loss as a crash. */
    esp_core_dump_image_erase();
}

const char *power_last_crash_task(void)
{
    return s_crash_task;
}

uint32_t power_last_crash_pc(void)
{
    return s_crash_pc;
}

void power_shutdown(void)
{
    ESP_LOGW(TAG, "shutdown: releasing EnableReg");
    if (s_shutdown_hook) s_shutdown_hook();   /* e.g. amp off -> DAC off, in the audio service's order */

    /* Leave the clean-off marker last, once nothing can fail the shutdown anymore. */
    nvs_handle_t h;
    if (nvs_open(OFF_NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, OFF_NVS_KEY, 1);
        nvs_commit(h);
        nvs_close(h);
    }

    gpio_set_level(PIN_REG_EN, 0);
    /* Rail collapses within the regulator's turn-off time. If USB still latches it
       on, power stays up; nothing more we can do. */
    for (;;) vTaskDelay(portMAX_DELAY);
}
