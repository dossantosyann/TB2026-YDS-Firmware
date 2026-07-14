/**
 * @file maintenance.c
 * @brief Maintenance task: periodically runs background chores (currently power_tick).
 * @ingroup services_maintenance
 */
#include "maintenance.h"
#include "power.h"
#include "volume.h"
#include "player.h"
#include "playlist.h"
#include "storage.h"
#include "sdcard.h"
#include "input.h"
#include "settings.h"
#include "diag.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAINT_TASK_PRIO   3      /* below input (5) and the audio task (22) */
#define MAINT_TASK_STACK  3072   /* fuel_gauge_read does a burst of I2C reads */
#define MAINT_VOL_MS      100    /* volume knob poll rate */
#define MAINT_PWR_DIV     20    /* power_tick every 2000 ms */
#define MAINT_SD_DIV      5     /* card-detect sample every 500 ms */

static const char *TAG = "maint";

/* Auto power-off: release the rail after an idle stretch without a button press, provided
   nothing is playing (a pause counts as not playing; its position is saved first).
   Skipped while USB is plugged in — the charger latches the regulator, so releasing
   EnableReg couldn't cut the rail anyway and would just park the firmware. The timeout is
   a user preference (power_get_poweroff_ms(), Settings > Power); 0 disables it. */

static TaskHandle_t s_task;

/* One-shot mount+scan on card insertion: same pattern as the boot scan in app.c —
   the recursive scan needs more stack than the maintenance task carries. */
static void sd_scan_task(void *arg)
{
    (void)arg;
    if (storage_init() == ESP_OK) {
        playlist_sync();
    }
    diag_task_exit();
    vTaskDelete(NULL);
}

/* SD hot-plug watcher: acts on debounced transitions of the card-detect pin (two
   agreeing samples 500 ms apart, which also lets the contacts seat on insertion).
   The first call only seeds the state — the boot-time mount is app.c's job. */
static void sd_watch(void)
{
    static bool seeded, prev_raw, present;

    bool raw = sdcard_present();
    if (!seeded) {
        prev_raw = present = raw;
        seeded = true;
        return;
    }
    bool stable = (raw == prev_raw);
    prev_raw = raw;
    if (!stable || raw == present) {
        return;
    }
    present = raw;

    if (present) {
        ESP_LOGI(TAG, "SD card inserted, mounting");
        if (xTaskCreate(sd_scan_task, "sd_scan", 4096, NULL, 1, NULL) != pdPASS) {
            ESP_LOGE(TAG, "sd_scan task alloc failed");
            present = false;   /* retry on the next stable sample */
        }
    } else {
        ESP_LOGW(TAG, "SD card removed, unmounting");
        player_stop();
        /* pipeline_stop is queued to the audio task (prio 22): give it time to
           process the command and close its file before the volume goes away. */
        vTaskDelay(pdMS_TO_TICKS(200));
        storage_unmount();
    }
}

static void power_off_watch(void)
{
    uint32_t off_ms = power_get_poweroff_ms();
    if (off_ms == 0) return;                      /* auto power-off disabled */
    if (input_idle_ms() < off_ms) return;

    player_status_t st;
    player_get_state(&st);
    if (st.state == PLAYER_PLAYING) return;

    power_state_t pw;
    power_get_state(&pw);
    if (pw.external_power) return;   /* USB latches the regulator: can't power off */

    ESP_LOGW(TAG, "idle with no playback, powering off");
    if (st.state == PLAYER_PAUSED) {
        /* Resume info for a future boot: the path survives an SD re-scan, an index
           wouldn't. Restoring it at boot is not wired yet. */
        settings_set_str("last_path", st.track.path);
        settings_set_u32("last_pos", st.elapsed_ms);
    }
    player_stop();
    /* pipeline_stop is queued to the audio task: give it time to process the
       command and close its file before the volume goes away (same as sd_watch). */
    vTaskDelay(pdMS_TO_TICKS(200));
    storage_unmount();
    power_shutdown();   /* runs the amp/DAC-off hook, releases EnableReg; no return */
}

static void maintenance_task(void *arg)
{
    (void)arg;
    diag_register_task(MAINT_TASK_STACK);
    const TickType_t period = pdMS_TO_TICKS(MAINT_VOL_MS);
    int pwr = 0, sd = 0;
    for (;;) {
        volume_poll(NULL, NULL);
        player_poll();
        if (++pwr >= MAINT_PWR_DIV) { power_tick(); power_off_watch(); pwr = 0; }
        if (++sd >= MAINT_SD_DIV) { sd_watch(); sd = 0; }
        diag_tick(MAINT_VOL_MS);   /* dumps the report every DIAG_DUMP_MS; no-op when off */
        vTaskDelay(period);
    }
}

esp_err_t maintenance_init(void)
{
    if (s_task) return ESP_OK;   /* idempotent */
    if (xTaskCreate(maintenance_task, "maint", MAINT_TASK_STACK, NULL,
                    MAINT_TASK_PRIO, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
