/**
 * @file autonomy.c
 * @brief Autonomy test service: run lifecycle, discharge-curve logging, SD export.
 * @ingroup services_autonomy
 */
#include "autonomy.h"
#include "power.h"
#include "storage.h"
#include "sdcard.h"
#include "player.h"
#include "playlist.h"
#include "volume.h"
#include "bluetooth.h"
#include "track_meta.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "esp_system.h"          /* esp_get_minimum_free_heap_size */
#include "esp_private/esp_clk.h" /* esp_clk_cpu_freq */
#include "esp_log.h"
#include "nvs.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static const char *TAG = "autonomy";

#define NVS_NS       "autonomy"
#define NVS_KEY_RUN  "run"
#define NVS_KEY_SEQ  "seq"          /* monotonic run counter, for unique CSV file names */
#define RUN_MAGIC    0xA5D50001u
#define RUN_VERSION  4              /* v4 added per-sample BT flag + min-heap, and run-start SD/CPU freq (v3: firmware version; v2: workload header) */
#define MAX_SAMPLES  256            /* blob stays ~2.5 KB — the NVS partition is only 24 KB */
#define SAMPLE_S_0   60             /* initial seconds between samples ("every minute") */

/* Fixed, pot-independent volumes so a run is reproducible. Jack drives the analog amp, whose
   draw scales with volume, so 50% is a representative mid load. Bluetooth amplifies in the
   external (separately powered) speaker, so the AVRCP level does not affect this board's draw —
   0% just keeps the speaker quiet without changing the measurement. */
#define JACK_VOL_PCT 50
#define BT_VOL_PCT   1

/* run_t.format */
#define FMT_NONE 0
#define FMT_MP3  1
#define FMT_WAV  2
#define FILE_MAX 48                 /* stored track name, truncated (header only, once per run) */

/* One point of the discharge curve. Elapsed time is stored per sample (not derived from the
   index) so it stays exact after a decimation halves the buffer. */
typedef struct __attribute__((packed)) {
    uint32_t t_s;       /* elapsed seconds from the run start */
    int16_t  mv;        /* cell voltage, mV */
    int16_t  soc_x10;   /* state of charge, % * 10 */
    int16_t  ma;        /* current, mA (negative = discharge) */
    uint8_t  bt;        /* Bluetooth radio powered at this sample (1) or off (0) */
    uint16_t heap_kb;   /* minimum free heap ever, KiB (watermark: a fall reveals a leak) */
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
    uint8_t  volume;        /* applied output level: DAC byte (jack) or AVRCP value (bt), 0 idle */
    uint8_t  format;        /* FMT_NONE / FMT_MP3 / FMT_WAV */
    uint32_t rate_hz;       /* source sample rate, 0 when idle */
    char     file[FILE_MAX];/* looped track display name, empty when idle */
    char     fw[32];        /* app version at run start (esp_app_desc), NOT at export: a reflash
                               between the run's death and the boot export must not relabel it */
    int32_t  sd_freq_khz;   /* negotiated SD SPI clock at run start, kHz (0 if no card) -- captured
                               at start, like fw: the boot export runs on a later boot */
    uint16_t cpu_freq_mhz;  /* CPU frequency at run start, MHz (same capture-at-start reason) */
    sample_t samples[MAX_SAMPLES];
} run_t;

static run_t s_run;   /* ~2.6 KB BSS: the working and persisted curve */

/* Runtime-only state. */
static bool    s_running;
static int64_t s_start_us;
static int64_t s_last_sample_us;

/* Saved so the workload teardown restores normal listening after a cancel/abort. */
static playlist_repeat_t s_prev_repeat;
static bool              s_workload_active;

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
    case AUTONOMY_RESULT_CANCELLED:   return "cancelled";
    case AUTONOMY_RESULT_COMPLETED:   return "completed";
    case AUTONOMY_RESULT_ABORTED:     return "aborted";
    case AUTONOMY_RESULT_INTERRUPTED: return "interrupted";
    default:                          return "none";
    }
}

static const char *format_name(uint8_t f)
{
    switch (f) {
    case FMT_MP3: return "mp3";
    case FMT_WAV: return "wav";
    default:      return "none";
    }
}

static uint8_t name_format(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return FMT_NONE;
    if (strcasecmp(dot, ".mp3") == 0) return FMT_MP3;
    if (strcasecmp(dot, ".wav") == 0) return FMT_WAV;
    return FMT_NONE;
}

static size_t blob_len(void)
{
    return offsetof(run_t, samples) + (size_t)s_run.count * sizeof(sample_t);
}

/* Set up the audio workload and record its metadata. IDLE stops playback; JACK and BT measure
   whatever is already playing on the respective output (the jack DAC, or a connected Bluetooth
   sink), pinning it down for the run (loop-one so it never ends early, a fixed pot-independent
   volume) and saving the prior repeat mode for the teardown. BT additionally requires an
   established link — without a sink the ESP32 (an A2DP source) transmits nothing, so the reading
   would miss the dominant RF+SBC cost. */
static esp_err_t workload_start(autonomy_test_t type)
{
    /* JACK and IDLE measure this board's own draw, so the radio must be off: a lingering BT
       session (connected then exited, or a link that never auto-shut-down) would otherwise
       inflate the reading -- the bug that made two "identical" jack runs differ ~2x. BT keeps
       the radio up (it is the output). */
    if (type != AUTONOMY_TEST_BT && bluetooth_is_powered()) {
        bluetooth_disconnect();   /* drop any link; the deferred auto-shutdown then powers it down */
        bluetooth_shutdown();     /* power down now if it was on but idle (shutdown refuses under a link) */
    }

    if (type == AUTONOMY_TEST_IDLE) {
        player_stop();                       /* IDLE = silence */
        return ESP_OK;
    }

    player_status_t st;
    if (player_get_state(&st) != ESP_OK || st.state != PLAYER_PLAYING)
        return ESP_ERR_INVALID_STATE;        /* nothing to measure: caller must start playback */

    int vol_pct;
    switch (type) {
    case AUTONOMY_TEST_JACK:
        vol_pct = JACK_VOL_PCT;
        break;
    case AUTONOMY_TEST_BT:
        if (!bluetooth_is_connected()) return ESP_ERR_INVALID_STATE;  /* no sink: reading is bogus */
        if (volume_get_output() != VOLUME_OUT_BT) {
            esp_err_t r = player_set_output(VOLUME_OUT_BT);   /* route the stream to BT ourselves */
            if (r != ESP_OK) return r;   /* e.g. speaker has not acked its volume yet: don't start blind */
        }
        vol_pct = BT_VOL_PCT;
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (st.track.name) {
        strncpy(s_run.file, st.track.name, FILE_MAX - 1);
        s_run.format = name_format(st.track.name);
    }
    track_meta_t m;
    if (st.track.path && track_meta_read(st.track.path, &m) == ESP_OK) s_run.rate_hz = m.rate_hz;

    s_prev_repeat     = playlist_get_repeat();
    s_workload_active = true;
    playlist_set_repeat(PLAYLIST_REPEAT_ONE); /* loop the single track until the battery dies */
    volume_set_fixed(vol_pct);                /* deterministic: bypass the pot */
    s_run.volume = volume_get_level();        /* the fixed level (DAC byte / AVRCP value), for the CSV */
    return ESP_OK;
}

/* Undo workload_start(): drop the fixed volume and restore the prior repeat mode. Playback is left
   running — the JACK run used the user's own stream. IDLE changed nothing, so this is a no-op. */
static void workload_stop(void)
{
    if (!s_workload_active) return;
    s_workload_active = false;
    volume_clear_fixed();
    playlist_set_repeat(s_prev_repeat);
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
    s->bt      = bluetooth_is_powered() ? 1 : 0;
    s->heap_kb = (uint16_t)(esp_get_minimum_free_heap_size() / 1024);
    persist();
}

static void finalize(autonomy_result_t result)
{
    workload_stop();                     /* stop playback and restore volume/output/repeat */
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
   name carries a monotonic counter (NVS) so a new run never overwrites an earlier one.
   end_cause labels how the board died mid-run (clean/crash/power-loss); it only makes sense
   for the boot export — an in-session export means the board never died — so pass NULL there. */
static esp_err_t export_csv(const char *end_cause)
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
    fprintf(f, "# fw,%s\n",              s_run.fw);
    fprintf(f, "# type,%s\n",            type_name(s_run.type));
    fprintf(f, "# result,%s\n",          result_name(s_run.result));
    fprintf(f, "# start_soc_pct,%.1f\n", s_run.start_soc_x10 / 10.0f);
    fprintf(f, "# file,%s\n",            s_run.file);
    fprintf(f, "# format,%s\n",          format_name(s_run.format));
    fprintf(f, "# rate_hz,%u\n",         (unsigned)s_run.rate_hz);
    fprintf(f, "# volume,%u\n",          s_run.volume);
    fprintf(f, "# sd_freq_khz,%d\n",     (int)s_run.sd_freq_khz);
    fprintf(f, "# cpu_freq_mhz,%u\n",    s_run.cpu_freq_mhz);
    fprintf(f, "# duration_s,%u\n",      (unsigned)duration_s());
    if (end_cause) fprintf(f, "# end,%s\n", end_cause);
    fprintf(f, "t_s,voltage_mV,soc_pct,current_mA,bt,heap_min_kb\n");
    for (uint16_t i = 0; i < s_run.count; i++) {
        const sample_t *s = &s_run.samples[i];
        fprintf(f, "%u,%d,%.1f,%d,%u,%u\n",
                (unsigned)s->t_s, s->mv, s->soc_x10 / 10.0f, s->ma, s->bt, s->heap_kb);
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
    s_run.result        = AUTONOMY_RESULT_INTERRUPTED; /* on-disk "in progress"; finalize() commits the real outcome */
    s_run.interval_s    = SAMPLE_S_0;
    s_run.start_soc_x10 = (int16_t)(p.soc_pct * 10.0f);
    strncpy(s_run.fw, esp_app_get_description()->version, sizeof s_run.fw - 1);
    s_run.sd_freq_khz  = sdcard_freq_khz();
    s_run.cpu_freq_mhz = (uint16_t)(esp_clk_cpu_freq() / 1000000);

    esp_err_t err = workload_start(type);
    if (err != ESP_OK) {                  /* e.g. nothing playing for JACK: don't start a bogus run */
        workload_stop();                  /* undo any partial volume/repeat setup */
        return err;
    }

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
    export_csv(NULL);   /* dump now (the device is on); if no card, stays pending for the next boot */
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
        export_csv(NULL);
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
    if (err != ESP_OK || s_run.magic != RUN_MAGIC || s_run.version != RUN_VERSION)
        return ESP_OK;                                                 /* nothing stored / old layout */

    /* Summary for the UI, whether or not it still needs exporting. */
    s_last_result     = (autonomy_result_t)s_run.result;
    s_last_type       = (autonomy_test_t)s_run.type;
    s_last_duration_s = duration_s();
    s_last_dump_ok    = s_run.exported != 0;

    if (s_run.exported) return ESP_OK;   /* already on the SD card (or a discarded abort) */

    /* Pending run = the board died while it ran. Label the death with the boot verdict:
       clean means the graceful battery-critical shutdown, anything else is the fault. */
    const char *end;
    switch (power_last_off_cause()) {
    case POWER_OFF_CLEAN: end = "clean";      break;
    case POWER_OFF_CRASH: end = "crash";      break;
    default:              end = "power-loss"; break;
    }
    return export_csv(end);              /* completed-by-shutdown runs land here on the next boot */
}

esp_err_t autonomy_redump(void)
{
    if (s_running) return ESP_ERR_INVALID_STATE;         /* a run owns s_run; don't re-dump a partial */
    if (s_run.magic != RUN_MAGIC) return ESP_ERR_NOT_FOUND;   /* no run loaded (none ever stored) */
    return export_csv(NULL);   /* writes a fresh uniquely-named CSV; confirms the SD write path */
}

autonomy_result_t autonomy_get_last_result(void) { return s_last_result; }
autonomy_test_t   autonomy_get_last_type(void)   { return s_last_type; }
uint32_t autonomy_get_last_duration_s(void)      { return s_last_duration_s; }
bool     autonomy_get_last_dump_ok(void)         { return s_last_dump_ok; }
