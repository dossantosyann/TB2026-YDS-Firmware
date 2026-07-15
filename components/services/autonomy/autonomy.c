/**
 * @file autonomy.c
 * @brief Autonomy test service: run lifecycle, discharge-curve logging, SD export.
 * @ingroup services_autonomy
 */
#include "autonomy.h"
#include "power.h"
#include "storage.h"
#include "sdcard.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "autonomy";

#define NVS_NS       "autonomy"
#define NVS_KEY_RUN  "run"
#define NVS_KEY_SEQ  "seq"          /* monotonic run counter, for unique CSV file names */
#define RUN_MAGIC    0xA5D50001u
#define RUN_VERSION  1
#define MAX_SAMPLES  256            /* blob stays ~2.5 KB — the NVS partition is only 24 KB */
#define SAMPLE_S_0   60             /* initial seconds between samples ("every minute") */

/* One point of the discharge curve. Elapsed time is stored per sample (not derived from the
   index) so it stays exact after a decimation halves the buffer. */
typedef struct __attribute__((packed)) {
    uint32_t t_s;       /* elapsed seconds from the run start */
    int16_t  mv;        /* cell voltage, mV */
    int16_t  soc_x10;   /* state of charge, % * 10 */
    int16_t  ma;        /* current, mA (negative = discharge) */
} sample_t;

/* The whole run, header + curve, persisted as one NVS blob (only count samples are written). */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  version;
    uint8_t  type;          /* autonomy_test_t */
    uint8_t  result;        /* autonomy_result_t (COMPLETED provisional while running) */
    uint8_t  exported;      /* 1 once written to the SD card */
    uint16_t interval_s;    /* current sampling interval (doubles when the buffer fills) */
    uint16_t count;         /* number of valid samples */
    int16_t  start_soc_x10; /* SOC at the start, % * 10 */
    uint8_t  volume;        /* workload volume (Phase C/D) */
    uint8_t  reserved;
    sample_t samples[MAX_SAMPLES];
} run_t;

static run_t s_run;   /* ~2.6 KB BSS: the working and persisted curve */

/* Runtime-only state. */
static bool    s_running;
static int64_t s_start_us;
static int64_t s_last_sample_us;

/* Last-run summary for the UI, set at boot (from NVS) and on cancel/finalize. */
static autonomy_result_t s_last_result = AUTONOMY_RESULT_NONE;
static autonomy_test_t   s_last_type;
static uint32_t          s_last_duration_s;
static bool              s_last_dump_ok;

static const char *type_name(uint8_t t)
{
    switch (t) {
    case AUTONOMY_TEST_IDLE: return "idle";
    case AUTONOMY_TEST_JACK: return "jack";
    case AUTONOMY_TEST_BT:   return "bt";
    default:                 return "?";
    }
}

static const char *result_name(uint8_t r)
{
    switch (r) {
    case AUTONOMY_RESULT_CANCELLED: return "cancelled";
    case AUTONOMY_RESULT_COMPLETED: return "completed";
    case AUTONOMY_RESULT_ABORTED:   return "aborted";
    default:                        return "none";
    }
}

static size_t blob_len(void)
{
    return offsetof(run_t, samples) + (size_t)s_run.count * sizeof(sample_t);
}

static void persist(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, NVS_KEY_RUN, &s_run, blob_len());
    nvs_commit(h);
    nvs_close(h);
}

static uint32_t duration_s(void)
{
    return s_run.count ? s_run.samples[s_run.count - 1].t_s : 0;
}

/* Append the current reading and persist. When the buffer is full, first halve it (keep every
   other point) and double the interval, so the curve keeps growing within a bounded blob. */
static void add_sample(const power_state_t *p)
{
    if (s_run.count >= MAX_SAMPLES) {
        for (uint16_t i = 0; i < MAX_SAMPLES / 2; i++) s_run.samples[i] = s_run.samples[2 * i];
        s_run.count = MAX_SAMPLES / 2;
        s_run.interval_s *= 2;
    }
    sample_t *s = &s_run.samples[s_run.count++];
    s->t_s     = (uint32_t)((esp_timer_get_time() - s_start_us) / 1000000);
    s->mv      = (int16_t)(p->voltage_v * 1000.0f);
    s->soc_x10 = (int16_t)(p->soc_pct * 10.0f);
    s->ma      = (int16_t)p->current_ma;
    persist();
}

static void finalize(autonomy_result_t result)
{
    s_run.result   = (uint8_t)result;
    s_run.exported = 0;
    persist();
    s_last_result     = result;
    s_last_type       = (autonomy_test_t)s_run.type;
    s_last_duration_s = duration_s();
    s_last_dump_ok    = false;   /* set true by export_csv() once written to the SD card */
    s_running = false;
    power_set_low_batt_shutdown(true);   /* restore the built-in critical shutdown */
}

/* Write the current run to a uniquely-named CSV on the SD card and mark it exported. The file
   name carries a monotonic counter (NVS) so a new run never overwrites an earlier one. */
static esp_err_t export_csv(void)
{
    if (!storage_ready()) return ESP_ERR_INVALID_STATE;   /* no card: retry at boot / re-dump */

    uint32_t seq = 0;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_get_u32(h, NVS_KEY_SEQ, &seq);   /* absent => 0 */
        nvs_close(h);
    }

    char path[48];
    snprintf(path, sizeof path, "%s/auton_%s_%03u.csv",
             SDCARD_MOUNT_POINT, type_name(s_run.type), (unsigned)seq);

    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "open %s failed", path);
        return ESP_FAIL;
    }
    fprintf(f, "# autonomy test\n");
    fprintf(f, "# type,%s\n",            type_name(s_run.type));
    fprintf(f, "# result,%s\n",          result_name(s_run.result));
    fprintf(f, "# start_soc_pct,%.1f\n", s_run.start_soc_x10 / 10.0f);
    fprintf(f, "# duration_s,%u\n",      (unsigned)duration_s());
    fprintf(f, "t_s,voltage_mV,soc_pct,current_mA\n");
    for (uint16_t i = 0; i < s_run.count; i++) {
        const sample_t *s = &s_run.samples[i];
        fprintf(f, "%u,%d,%.1f,%d\n", (unsigned)s->t_s, s->mv, s->soc_x10 / 10.0f, s->ma);
    }
    fclose(f);

    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, NVS_KEY_SEQ, seq + 1);   /* next run gets a fresh name */
        nvs_commit(h);
        nvs_close(h);
    }
    s_run.exported = 1;
    persist();
    s_last_dump_ok = true;
    ESP_LOGI(TAG, "exported run to %s (%u samples)", path, (unsigned)s_run.count);
    return ESP_OK;
}

esp_err_t autonomy_start(autonomy_test_t type)
{
    if (s_running) return ESP_ERR_INVALID_STATE;

    power_state_t p;
    power_get_state(&p);

    memset(&s_run, 0, sizeof s_run);
    s_run.magic         = RUN_MAGIC;
    s_run.version       = RUN_VERSION;
    s_run.type          = (uint8_t)type;
    s_run.result        = AUTONOMY_RESULT_COMPLETED;   /* provisional; cancel overwrites */
    s_run.interval_s    = SAMPLE_S_0;
    s_run.start_soc_x10 = (int16_t)(p.soc_pct * 10.0f);

    s_start_us       = esp_timer_get_time();
    s_last_sample_us = s_start_us;
    s_running        = true;
    power_set_low_batt_shutdown(false);   /* the service owns the end now */

    add_sample(&p);   /* first point at t=0 */
    return ESP_OK;
}

void autonomy_cancel(void)
{
    if (!s_running) return;
    finalize(AUTONOMY_RESULT_CANCELLED);
    export_csv();   /* dump now (the device is on); if no card, stays pending for the next boot */
}

bool autonomy_is_running(void)
{
    return s_running;
}

void autonomy_get_status(autonomy_status_t *out)
{
    if (!out) return;
    out->running    = s_running;
    out->type       = (autonomy_test_t)s_run.type;
    out->elapsed_ms = s_running ? (uint32_t)((esp_timer_get_time() - s_start_us) / 1000) : 0;
}

void autonomy_tick(void)
{
    if (!s_running) return;

    power_state_t p;
    power_get_state(&p);
    if (!p.valid) return;

    /* USB plugged mid-run: the full-autonomy measurement is invalid (the cell stopped
       discharging), but the partial curve is still real data, so export it anyway — labelled
       "aborted" in the CSV header. The UI shows a big error. SD is up (USB power). */
    if (p.external_power) {
        finalize(AUTONOMY_RESULT_ABORTED);
        export_csv();
        ESP_LOGW(TAG, "USB plugged mid-run, aborting");
        return;
    }

    /* Own the end: same condition as power.c's built-in shutdown (which we suspended). Take a
       final point, write the log, then power off (never returns). */
    if (p.level == POWER_LEVEL_CRITICAL && !p.charging && !p.external_power) {
        add_sample(&p);
        finalize(AUTONOMY_RESULT_COMPLETED);
        ESP_LOGW(TAG, "battery critical, ending run after %lus", (unsigned long)s_last_duration_s);
        power_shutdown();
    }

    int64_t now = esp_timer_get_time();
    if (now - s_last_sample_us >= (int64_t)s_run.interval_s * 1000000) {
        s_last_sample_us = now;
        add_sample(&p);
    }
}

esp_err_t autonomy_boot_export(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return ESP_OK;   /* namespace never created */
    size_t len = sizeof s_run;
    esp_err_t err = nvs_get_blob(h, NVS_KEY_RUN, &s_run, &len);
    nvs_close(h);
    if (err != ESP_OK || s_run.magic != RUN_MAGIC) return ESP_OK;      /* nothing stored */

    /* Summary for the UI, whether or not it still needs exporting. */
    s_last_result     = (autonomy_result_t)s_run.result;
    s_last_type       = (autonomy_test_t)s_run.type;
    s_last_duration_s = duration_s();
    s_last_dump_ok    = s_run.exported != 0;

    if (s_run.exported) return ESP_OK;   /* already on the SD card (or a discarded abort) */
    return export_csv();                 /* completed-by-shutdown runs land here on the next boot */
}

autonomy_result_t autonomy_get_last_result(void) { return s_last_result; }
autonomy_test_t   autonomy_get_last_type(void)   { return s_last_type; }
uint32_t autonomy_get_last_duration_s(void)      { return s_last_duration_s; }
bool     autonomy_get_last_dump_ok(void)         { return s_last_dump_ok; }
