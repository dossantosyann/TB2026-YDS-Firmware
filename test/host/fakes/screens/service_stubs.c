/* Canned-data service stubs for the screenshot tool (test/host/screens_snapshot.c).
   Every screen .c file is compiled for real; every service/driver it calls is faked here with
   plausible fixed data, so a screen renders something representative without any hardware or
   ESP-IDF. Mutating calls (player_pause, bluetooth_connect, power_shutdown, ...) are only ever
   reached from handle_input, which the screenshot tool never calls -- they are no-ops. */
#include "player.h"
#include "playlist.h"
#include "volume.h"
#include "track_meta.h"
#include "adc.h"
#include "bluetooth.h"
#include "power.h"
#include "settings.h"
#include "display_oled.h"
#include "storage.h"
#include "sdcard.h"
#include "gpio_expander.h"
#include "audio_dac.h"
#include "i2c_bus.h"
#include "driver/i2c_master.h"
#include "input.h"
#include "fuel_gauge.h"
#include "autonomy.h"

#include "esp_timer.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_idf_version.h"

#include <string.h>

/* ---- shared fixture: the playback queue (also what player_get_state()/playlist_current()
   report as "now playing" for track 0). Independent of what's on disk under
   test/host/fixtures/sdcard -- that's a separate artist-folder library for storage_screen's own
   opendir() browse (see fakes/screens/sdcard.h); track_meta_read()/storage_track_*() below never
   touch the filesystem, so these paths don't need to exist. Artist/album are left blank where the
   real attribution isn't known; playlist_browser only ever displays the name column anyway. */
#define ROOT "test/host/fixtures/queue/"

typedef struct { const char *path, *name, *title, *artist, *album;
                 uint32_t duration_ms, rate_hz, bitrate_bps; uint8_t bits; } fixture_track_t;

static const fixture_track_t k_lib[] = {
    { ROOT "01.mp3", "Specter.mp3",                  "Specter",                  "Bad Omens",             "Specter", 227000, 44100, 320000, 16 },
    { ROOT "02.mp3", "Can You Feel My Heart.mp3",     "Can You Feel My Heart",    "Bring Me The Horizon",  "Sempiternal", 256000, 44100, 256000, 16 },
    { ROOT "03.mp3", "Follow You.mp3",                "Follow You",               "Bring Me The Horizon",  "amo", 214000, 44100, 256000, 16 },
    { ROOT "04.mp3", "To The Flowers.mp3",            "To The Flowers",           "",                      "", 200000, 44100, 256000, 16 },
    { ROOT "05.mp3", "Doomed.mp3",                    "Doomed",                   "Bring Me The Horizon",  "Sempiternal", 236000, 44100, 256000, 16 },
    { ROOT "06.mp3", "Impose.mp3",                    "Impose",                   "",                      "", 200000, 44100, 256000, 16 },
    { ROOT "07.mp3", "Be Very Afraid.mp3",            "Be Very Afraid",           "",                      "", 200000, 44100, 256000, 16 },
    { ROOT "08.mp3", "Dying To Love.mp3",             "Dying To Love",            "",                      "", 200000, 44100, 256000, 16 },
    { ROOT "09.mp3", "The Emptiness Machine.mp3",     "The Emptiness Machine",    "Bring Me The Horizon",  "Post Human: Nex Gen", 213000, 44100, 320000, 16 },
    { ROOT "10.mp3", "Heavy Is The Crown.mp3",        "Heavy Is The Crown",       "Bring Me The Horizon",  "Post Human: Nex Gen", 199000, 44100, 320000, 16 },
};
#define N_LIB (int)(sizeof k_lib / sizeof k_lib[0])

/* ---- player ---------------------------------------------------------------------- */

esp_err_t player_get_state(player_status_t *out)
{
    out->state = PLAYER_PLAYING;
    out->track.index = 0;
    out->track.path  = k_lib[0].path;
    out->track.name  = k_lib[0].name;
    out->elapsed_ms  = 96000;
    out->total_ms    = k_lib[0].duration_ms;
    out->track_unsupported = false;
    return ESP_OK;
}
esp_err_t player_set_output(volume_output_t out) { (void)out; return ESP_OK; }
void      player_poll(void) {}
esp_err_t player_play(size_t index)  { (void)index; return ESP_OK; }
esp_err_t player_start(void)         { return ESP_OK; }
esp_err_t player_pause(void)         { return ESP_OK; }
esp_err_t player_resume(void)        { return ESP_OK; }
esp_err_t player_stop(void)          { return ESP_OK; }
esp_err_t player_next(void)          { return ESP_OK; }
esp_err_t player_prev(void)          { return ESP_OK; }

/* ---- playlist --------------------------------------------------------------------- */

esp_err_t playlist_sync(void) { return ESP_OK; }

esp_err_t playlist_current(playlist_track_t *out)
{
    out->index = 0; out->path = k_lib[0].path; out->name = k_lib[0].name;
    return ESP_OK;
}
esp_err_t playlist_next(playlist_track_t *out) { return playlist_current(out); }
esp_err_t playlist_prev(playlist_track_t *out) { return playlist_current(out); }
esp_err_t playlist_select(size_t index, playlist_track_t *out) { (void)index; return playlist_current(out); }
esp_err_t playlist_random(playlist_track_t *out) { return playlist_current(out); }

static playlist_repeat_t s_repeat = PLAYLIST_REPEAT_ALL;
void playlist_set_repeat(playlist_repeat_t mode) { s_repeat = mode; }
playlist_repeat_t playlist_get_repeat(void) { return s_repeat; }

static bool s_shuffle = false;
void playlist_set_shuffle(bool on) { s_shuffle = on; }
bool playlist_get_shuffle(void) { return s_shuffle; }

size_t playlist_queue_len(void) { return (size_t)N_LIB; }

esp_err_t playlist_queue_at(size_t row, playlist_track_t *out)
{
    if (row >= (size_t)N_LIB) return ESP_ERR_NOT_FOUND;
    out->index = row; out->path = k_lib[row].path; out->name = k_lib[row].name;
    return ESP_OK;
}
esp_err_t playlist_queue_move_up(size_t row)   { (void)row; return ESP_OK; }
esp_err_t playlist_queue_move_down(size_t row) { (void)row; return ESP_OK; }
esp_err_t playlist_queue_remove(size_t row)    { (void)row; return ESP_OK; }

/* ---- volume ------------------------------------------------------------------------ */

static volume_output_t s_vol_out = VOLUME_OUT_DAC;
volume_output_t volume_get_output(void)       { return s_vol_out; }
void            volume_set_output(volume_output_t o) { s_vol_out = o; }

bool volume_poll(int *out_mv, uint8_t *out_level)
{
    if (out_mv)    *out_mv = 2200;
    if (out_level) *out_level = 90;
    return true;
}
bool volume_read_mv(int *out_mv) { if (out_mv) *out_mv = 2200; return true; }

static int8_t s_balance = 0;
int8_t volume_get_balance(void)          { return s_balance; }
void   volume_set_balance(int8_t steps)  { s_balance = steps; }
void   volume_save_balance(void)         {}
void   volume_set_fixed(int percent)     { (void)percent; }
void   volume_clear_fixed(void)          {}
uint8_t volume_get_level(void)           { return 90; }
void   volume_set_bt_handler(esp_err_t (*f)(uint8_t)) { (void)f; }

/* ---- track_meta ---------------------------------------------------------------------- */

esp_err_t track_meta_read(const char *path, track_meta_t *out)
{
    for (int i = 0; i < N_LIB; i++) {
        if (strcmp(path, k_lib[i].path) != 0) continue;
        strncpy(out->title,  k_lib[i].title,  TRACK_META_STR_MAX - 1);
        strncpy(out->artist, k_lib[i].artist, TRACK_META_STR_MAX - 1);
        strncpy(out->album,  k_lib[i].album,  TRACK_META_STR_MAX - 1);
        out->duration_ms = k_lib[i].duration_ms;
        out->rate_hz     = k_lib[i].rate_hz;
        out->bits        = k_lib[i].bits;
        out->bitrate_bps = k_lib[i].bitrate_bps;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

/* ---- adc (pure mapping, no hardware) --------------------------------------------------- */

uint8_t adc_pot_to_avrcp_volume(int mv)
{
    if (mv < 0) mv = 0;
    if (mv > 3300) mv = 3300;
    return (uint8_t)(mv * 0x7F / 3300);
}

/* ---- bluetooth ------------------------------------------------------------------------ */

static bt_known_device_t s_known[] = {
    { .bda = {0x11,0x22,0x33,0x44,0x55,0x66}, .name = "JBL Flip 5" },
    { .bda = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF}, .name = "Car Kit" },
};
static bluetooth_device_t s_near[] = {
    { .bda = {0x11,0x22,0x33,0x44,0x55,0x66}, .name = "JBL Flip 5",        .rssi = -52 },
    { .bda = {0x77,0x88,0x99,0xAA,0xBB,0xCC}, .name = "Living Room Sonos", .rssi = -68 },
};

bool bluetooth_is_connected(void) { return true; }
bool bluetooth_is_scanning(void)  { return false; }
bool bluetooth_is_powered(void)   { return true; }
bluetooth_conn_state_t bluetooth_get_conn_state(void) { return BLUETOOTH_CONN_CONNECTED; }

size_t bluetooth_get_devices(bluetooth_device_t *out, size_t cap)
{
    size_t n = sizeof s_near / sizeof s_near[0];
    if (n > cap) n = cap;
    if (out) memcpy(out, s_near, n * sizeof *out);
    return n;
}
size_t bluetooth_get_known_devices(bt_known_device_t *out, size_t cap)
{
    size_t n = sizeof s_known / sizeof s_known[0];
    if (n > cap) n = cap;
    if (out) memcpy(out, s_known, n * sizeof *out);
    return n;
}
bool bluetooth_get_last_device(bt_known_device_t *out)
{
    if (out) *out = s_known[0];
    return true;
}
void bluetooth_set_last_device(const bt_known_device_t *dev) { (void)dev; }

esp_err_t bluetooth_init(void)          { return ESP_OK; }
esp_err_t bluetooth_shutdown(void)      { return ESP_OK; }
void      bluetooth_set_auto_shutdown(bool enable) { (void)enable; }
esp_err_t bluetooth_load_known(void)    { return ESP_OK; }
esp_err_t bluetooth_scan_start(void)    { return ESP_OK; }
esp_err_t bluetooth_scan_stop(void)     { return ESP_OK; }
esp_err_t bluetooth_connect(const esp_bd_addr_t bda) { (void)bda; return ESP_OK; }
esp_err_t bluetooth_connect_by_name(const char *s)   { (void)s; return ESP_OK; }
esp_err_t bluetooth_disconnect(void)    { return ESP_OK; }
esp_err_t bluetooth_reconnect_last(void){ return ESP_OK; }
esp_err_t bluetooth_forget_device(const esp_bd_addr_t bda) { (void)bda; return ESP_OK; }
esp_err_t bluetooth_audio_start(esp_a2d_source_data_cb_t cb) { (void)cb; return ESP_OK; }
esp_err_t bluetooth_audio_stop(void)    { return ESP_OK; }
esp_err_t bluetooth_set_absolute_volume(uint8_t v) { (void)v; return ESP_OK; }
bool      bluetooth_is_avrc_connected(void) { return true; }
bool      bluetooth_volume_acked(void)      { return true; }

/* ---- power ------------------------------------------------------------------------------ */

void power_get_state(power_state_t *out)
{
    out->valid          = true;
    out->soc_pct        = 76.0f;
    out->voltage_v       = 3.87f;
    out->current_ma      = -120.0f;
    out->charging        = false;
    out->external_power  = false;
    out->tte_s           = 14400.0f;
    out->level           = POWER_LEVEL_NORMAL;
}
void power_self_hold(void) {}
void power_tick(void) {}
void power_set_usb_route(power_usb_route_t r) { (void)r; }
static power_usb_route_t s_usb_route = POWER_USB_DATA;
power_usb_route_t power_get_usb_route(void) { return s_usb_route; }
void power_usb_autoroute_start(void) {}
void power_set_low_batt_shutdown(bool enable) { (void)enable; }
void power_set_shutdown_hook(power_shutdown_hook_t hook) { (void)hook; }
void power_shutdown(void) { for (;;) {} }  /* noreturn; never actually invoked */
void power_boot_off_check(void) {}
power_off_cause_t power_last_off_cause(void) { return POWER_OFF_CLEAN; }
const char *power_last_crash_task(void) { return ""; }
uint32_t power_last_crash_pc(void) { return 0; }

static uint32_t s_sleep_ms = 60000, s_poweroff_ms = 300000;
uint32_t power_get_sleep_ms(void)    { return s_sleep_ms; }
void     power_set_sleep_ms(uint32_t ms) { s_sleep_ms = ms; }
uint32_t power_get_poweroff_ms(void) { return s_poweroff_ms; }
void     power_set_poweroff_ms(uint32_t ms) { s_poweroff_ms = ms; }

/* ---- settings (NVS-backed prefs) --------------------------------------------------------- */

esp_err_t settings_init(void) { return ESP_OK; }
esp_err_t settings_get_u8(const char *key, uint8_t *out)  { (void)key; (void)out; return ESP_ERR_NOT_FOUND; }
esp_err_t settings_set_u8(const char *key, uint8_t val)   { (void)key; (void)val; return ESP_OK; }
esp_err_t settings_get_u32(const char *key, uint32_t *out){ (void)key; (void)out; return ESP_ERR_NOT_FOUND; }
esp_err_t settings_set_u32(const char *key, uint32_t val) { (void)key; (void)val; return ESP_OK; }
esp_err_t settings_get_str(const char *key, char *out, size_t out_size)
{ (void)key; if (out_size) out[0] = '\0'; return ESP_ERR_NOT_FOUND; }
esp_err_t settings_set_str(const char *key, const char *val) { (void)key; (void)val; return ESP_OK; }

/* ---- display_oled (gfx_flush's target + brightness) -------------------------------------- */

esp_err_t display_oled_draw(const uint8_t *buf, size_t len) { (void)buf; (void)len; return ESP_OK; }
esp_err_t display_oled_set_brightness(uint8_t level) { (void)level; return ESP_OK; }
esp_err_t display_oled_sleep(void) { return ESP_OK; }
esp_err_t display_oled_wake(void)  { return ESP_OK; }

/* ---- storage (index over the SD card) ----------------------------------------------------- */

bool sdcard_present(void)  { return true; }
int  sdcard_freq_khz(void) { return 20000; }
esp_err_t sdcard_mount(void)   { return ESP_OK; }
esp_err_t sdcard_unmount(void) { return ESP_OK; }

bool   storage_ready(void) { return true; }
size_t storage_count(void) { return (size_t)N_LIB; }
esp_err_t storage_get_usage(uint64_t *total_bytes, uint64_t *used_bytes)
{
    if (total_bytes) *total_bytes = 32ULL * 1024 * 1024 * 1024;
    if (used_bytes)  *used_bytes  = 12ULL * 1024 * 1024 * 1024;
    return ESP_OK;
}
const char *storage_track_path(size_t index) { return index < (size_t)N_LIB ? k_lib[index].path : NULL; }
const char *storage_track_name(size_t index) { return index < (size_t)N_LIB ? k_lib[index].name : NULL; }
esp_err_t storage_rescan(void)     { return ESP_OK; }
esp_err_t storage_scan_dir(const char *root) { (void)root; return ESP_OK; }
esp_err_t storage_deinit(void)     { return ESP_OK; }

/* ---- gpio_expander --------------------------------------------------------------------- */

esp_err_t gpio_expander_get(uint8_t channel, bool *level)
{
    *level = (channel == 7);   /* EXP_INOKB: high = no external source, matches power state */
    return ESP_OK;
}
esp_err_t gpio_expander_set(uint8_t channel, bool level) { (void)channel; (void)level; return ESP_OK; }
esp_err_t gpio_expander_read_all(uint8_t *inputs) { *inputs = 0x80; return ESP_OK; }

/* ---- audio_dac ------------------------------------------------------------------------- */

bool audio_dac_ready(void) { return true; }
esp_err_t audio_dac_get_clock_status(uint8_t *status) { *status = 0x00; return ESP_OK; }
esp_err_t audio_dac_set_volume(uint8_t l, uint8_t r) { (void)l; (void)r; return ESP_OK; }
esp_err_t audio_dac_get_volume(uint8_t *level) { *level = 0x30; return ESP_OK; }
esp_err_t audio_dac_mute(bool mute) { (void)mute; return ESP_OK; }
esp_err_t audio_dac_standby(bool standby) { (void)standby; return ESP_OK; }

/* ---- i2c_bus / driver/i2c_master --------------------------------------------------------- */

i2c_master_bus_handle_t i2c_bus_handle(void) { return (i2c_master_bus_handle_t)1; }
void i2c_bus_scan(i2c_master_bus_handle_t bus) { (void)bus; }

esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus_handle, uint16_t address, int xfer_timeout_ms)
{
    (void)bus_handle; (void)xfer_timeout_ms;
    return (address == 0x36 || address == 0x38 || address == 0x4C) ? ESP_OK : ESP_FAIL;
}

/* ---- input diagnostics ------------------------------------------------------------------- */

void input_get_diag(input_diag_t *out)
{
    memset(out, 0, sizeof *out);
    static const uint32_t counts[INPUT_BTN_COUNT] = { 12, 8, 3, 3, 20, 45 };
    memcpy(out->count, counts, sizeof counts);
}
uint32_t input_idle_ms(void) { return 4200; }

/* ---- fuel_gauge ------------------------------------------------------------------------- */

esp_err_t fuel_gauge_read(fuel_gauge_data_t *out)
{
    out->soc_pct     = 76.0f;
    out->voltage_v    = 3.87f;
    out->current_ma   = -120.0f;
    out->rep_cap_mah  = 950.0f;
    out->tte_s        = 14400.0f;
    out->ttf_s        = 0.0f;
    out->age_pct      = 97.0f;
    out->die_temp_c   = 28.5f;
    out->cycles       = 12.3f;
    return ESP_OK;
}
esp_err_t fuel_gauge_read_debug(fuel_gauge_debug_t *out)
{
    out->por              = false;
    out->design_cap_raw   = out->design_cap_want = 1000;
    out->ichg_term_raw    = out->ichg_term_want  = 50;
    out->vempty_raw       = out->vempty_want     = 0x0A5C;
    out->full_cap_rep_mah = 970.0f;
    out->full_cap_nom_mah = 985.0f;
    return ESP_OK;
}
esp_err_t fuel_gauge_reload_ez(void) { return ESP_OK; }

/* ---- autonomy test ------------------------------------------------------------------------ */

esp_err_t autonomy_start(autonomy_test_t type)  { (void)type; return ESP_OK; }
void      autonomy_cancel(void)                 {}
bool      autonomy_is_running(void)             { return false; }
void      autonomy_get_status(autonomy_status_t *out) { if (out) memset(out, 0, sizeof *out); }
void      autonomy_tick(void)                   {}
esp_err_t autonomy_boot_export(void)            { return ESP_OK; }
esp_err_t autonomy_redump(void)                 { return ESP_OK; }
autonomy_result_t autonomy_get_last_result(void)     { return AUTONOMY_RESULT_COMPLETED; }
autonomy_test_t   autonomy_get_last_type(void)       { return AUTONOMY_TEST_JACK; }
uint32_t          autonomy_get_last_duration_s(void) { return 26700; }
bool              autonomy_get_last_dump_ok(void)    { return true; }

/* ---- esp-system diagnostics (stats_screen's System page) ---------------------------------- */

int64_t  esp_timer_get_time(void)            { return (int64_t)5 * 3600 * 1000000; /* 5h uptime */ }
uint32_t esp_get_free_heap_size(void)        { return 3u * 1024 * 1024; }
uint32_t esp_get_minimum_free_heap_size(void){ return 2u * 1024 * 1024; }
esp_reset_reason_t esp_reset_reason(void)    { return ESP_RST_POWERON; }
const char *esp_get_idf_version(void)        { return "v6.0.1"; }

static const esp_app_desc_t k_app_desc = { .version = "1.0.0" };
const esp_app_desc_t *esp_app_get_description(void) { return &k_app_desc; }
