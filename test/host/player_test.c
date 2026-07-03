/* Host unit test for the playback transport state machine (player.c).
   Drives player.c over a FAKE pipeline (records play_file/stop/pause/resume and lets the test
   fire the end-of-track callback), a FAKE playlist (small list + repeat mode), and fake
   storage/bluetooth/sink/volume hooks. No ESP-IDF, no hardware. Checks: play -> playing, EOF
   auto-advance, next/prev reopen, end-of-list stop per repeat mode, repeat-one replay,
   pause/resume, fail-loud errors, and the Bluetooth volume-ack safety gate. */
#include "player.h"
#include "pipeline.h"
#include "audio_sink.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ---- fake playlist (mirrors playlist.h repeat semantics at the ends) ---------------------- */
static size_t            g_count, g_pos;
static bool              g_ready;
static playlist_repeat_t g_repeat;
static char              g_path[8][16], g_name[8][8];

static void pl_set(size_t n, playlist_repeat_t rep, bool ready)
{
    assert(n <= 8);
    g_count = n; g_pos = 0; g_repeat = rep; g_ready = ready;
    for (size_t i = 0; i < n; i++) {
        snprintf(g_path[i], sizeof g_path[i], "/sd/t%zu", i);
        snprintf(g_name[i], sizeof g_name[i], "t%zu", i);
    }
}
static void fill(playlist_track_t *o)
{
    if (!o) return;
    o->index = g_pos; o->path = g_path[g_pos]; o->name = g_name[g_pos];
}

esp_err_t playlist_current(playlist_track_t *o)
{
    if (!g_ready) return ESP_ERR_INVALID_STATE;
    if (g_count == 0) return ESP_ERR_NOT_FOUND;
    fill(o); return ESP_OK;
}
esp_err_t playlist_next(playlist_track_t *o)
{
    if (!g_ready) return ESP_ERR_INVALID_STATE;
    if (g_count == 0) return ESP_ERR_NOT_FOUND;
    if (g_pos + 1 < g_count) g_pos++;
    else if (g_repeat == PLAYLIST_REPEAT_OFF) return ESP_ERR_NOT_FOUND;
    else g_pos = 0;
    fill(o); return ESP_OK;
}
esp_err_t playlist_prev(playlist_track_t *o)
{
    if (!g_ready) return ESP_ERR_INVALID_STATE;
    if (g_count == 0) return ESP_ERR_NOT_FOUND;
    if (g_pos > 0) g_pos--;
    else if (g_repeat == PLAYLIST_REPEAT_OFF) return ESP_ERR_NOT_FOUND;
    else g_pos = g_count - 1;
    fill(o); return ESP_OK;
}
esp_err_t playlist_select(size_t i, playlist_track_t *o)
{
    if (!g_ready) return ESP_ERR_INVALID_STATE;
    if (i >= g_count) return ESP_ERR_INVALID_ARG;
    g_pos = i; fill(o); return ESP_OK;
}
playlist_repeat_t playlist_get_repeat(void) { return g_repeat; }
bool playlist_get_shuffle(void) { return false; }   /* transport tests run in list order */
esp_err_t playlist_random(playlist_track_t *o)
{
    if (!g_ready || g_count == 0) return ESP_ERR_INVALID_STATE;
    g_pos = 0;                                       /* deterministic "random" for the tests */
    if (o) fill(o);
    return ESP_OK;
}

/* ---- fake pipeline ----------------------------------------------------------------------- */
static void (*g_end_cb)(pipeline_end_reason_t);
static int      g_play, g_stop, g_pause, g_resume;
static char     g_last_path[32];
static esp_err_t g_play_ret;

void pipeline_set_track_end_cb(void (*cb)(pipeline_end_reason_t)) { g_end_cb = cb; }
esp_err_t pipeline_play_file(const char *path)
{
    if (g_play_ret != ESP_OK) return g_play_ret;
    g_play++; strncpy(g_last_path, path, sizeof g_last_path - 1); return ESP_OK;
}
esp_err_t pipeline_stop(void)   { g_stop++;   return ESP_OK; }
esp_err_t pipeline_pause(void)  { g_pause++;  return ESP_OK; }
esp_err_t pipeline_resume(void) { g_resume++; return ESP_OK; }
void pipeline_set_sink(const audio_sink_t *s) { (void)s; }
esp_err_t pipeline_switch_sink(void) { return ESP_OK; }
void pipeline_get_position(uint32_t *e, uint32_t *t) { if (e) *e = 0; if (t) *t = 0; }

static void pipe_reset(void) { g_play = g_stop = g_pause = g_resume = 0; g_play_ret = ESP_OK; g_last_path[0] = 0; }

/* ---- other fakes ------------------------------------------------------------------------- */
static bool g_storage_ready = true, g_bt_acked = true, g_bt_connected = true;
static volume_output_t g_vol_out;

bool storage_ready(void)        { return g_storage_ready; }
bool bluetooth_volume_acked(void) { return g_bt_acked; }
bool bluetooth_is_connected(void) { return g_bt_connected; }
void volume_set_output(volume_output_t o) { g_vol_out = o; }

/* player_init() wires the volume service's BT output to the AVRCP setter; the transport logic
   under test never calls either, so these only need to satisfy the linker. */
void volume_set_bt_handler(esp_err_t (*f)(uint8_t)) { (void)f; }
esp_err_t bluetooth_set_absolute_volume(uint8_t v) { (void)v; return ESP_OK; }

static const audio_sink_t g_dac, g_bt;
const audio_sink_t *sink_i2s_dac_get(void)  { return &g_dac; }
const audio_sink_t *sink_bluetooth_get(void) { return &g_bt; }

/* ---- helpers ----------------------------------------------------------------------------- */
static player_state_t state(void)
{
    player_status_t s; assert(player_get_state(&s) == ESP_OK); return s.state;
}

/* ---- tests ------------------------------------------------------------------------------- */
static void test_play_then_playing(void)
{
    pl_set(3, PLAYLIST_REPEAT_ALL, true); pipe_reset();
    assert(player_init() == ESP_OK);
    assert(player_set_output(VOLUME_OUT_DAC) == ESP_OK);

    assert(player_play(0) == ESP_OK);
    assert(state() == PLAYER_PLAYING);
    assert(g_stop == 1 && g_play == 1);            /* stopped any current stream, then started */
    assert(strcmp(g_last_path, "/sd/t0") == 0);

    player_status_t s; player_get_state(&s);
    assert(s.track.index == 0 && strcmp(s.track.name, "t0") == 0);
    printf("player: play -> playing, current track exposed OK\n");
}

static void test_auto_advance_on_eof(void)
{
    pl_set(3, PLAYLIST_REPEAT_ALL, true); pipe_reset();
    player_init();
    assert(player_play(0) == ESP_OK);

    pipe_reset();
    g_end_cb(PIPE_END_EOF);                         /* track finished naturally */
    assert(state() == PLAYER_PLAYING);
    assert(g_play == 1 && strcmp(g_last_path, "/sd/t1") == 0);   /* next track opened */
    printf("player: EOF auto-advances to the next track OK\n");
}

static void test_next_prev(void)
{
    pl_set(3, PLAYLIST_REPEAT_ALL, true); pipe_reset();
    player_init(); player_play(0);

    pipe_reset();
    assert(player_next() == ESP_OK);
    assert(g_stop == 1 && strcmp(g_last_path, "/sd/t1") == 0);

    pipe_reset();
    assert(player_prev() == ESP_OK);
    assert(g_stop == 1 && strcmp(g_last_path, "/sd/t0") == 0);
    printf("player: next/prev reopen the decoder on the new track OK\n");
}

static void test_repeat_off_end(void)
{
    pl_set(3, PLAYLIST_REPEAT_OFF, true); pipe_reset();
    player_init();
    assert(player_play(2) == ESP_OK);               /* sit on the last track */

    pipe_reset();
    g_end_cb(PIPE_END_EOF);                         /* end of list, repeat off */
    assert(state() == PLAYER_STOPPED);
    assert(g_play == 0);                            /* nothing new opened */
    printf("player: end of list with repeat off -> clean stop OK\n");
}

static void test_repeat_one(void)
{
    pl_set(3, PLAYLIST_REPEAT_ONE, true); pipe_reset();
    player_init();
    assert(player_play(1) == ESP_OK);

    pipe_reset();
    g_end_cb(PIPE_END_EOF);
    assert(state() == PLAYER_PLAYING);
    assert(g_play == 1 && strcmp(g_last_path, "/sd/t1") == 0);   /* same track replayed */
    printf("player: repeat-one replays the current track on EOF OK\n");
}

static void test_pause_resume(void)
{
    pl_set(3, PLAYLIST_REPEAT_ALL, true); pipe_reset();
    player_init(); player_play(0);

    assert(player_pause() == ESP_OK);
    assert(state() == PLAYER_PAUSED && g_pause == 1);
    assert(player_pause() == ESP_ERR_INVALID_STATE);   /* not playing anymore */

    assert(player_resume() == ESP_OK);
    assert(state() == PLAYER_PLAYING && g_resume == 1);
    assert(player_resume() == ESP_ERR_INVALID_STATE);  /* not paused */
    printf("player: pause -> paused, resume -> playing OK\n");
}

static void test_errors(void)
{
    /* storage not ready: refuse loud, current playback untouched. */
    pl_set(3, PLAYLIST_REPEAT_ALL, false); g_storage_ready = false; pipe_reset();
    player_init(); player_stop();                   /* known STOPPED baseline */
    assert(player_play(0) == ESP_ERR_INVALID_STATE);
    assert(g_play == 0 && state() == PLAYER_STOPPED);
    g_storage_ready = true;

    /* empty list: select out of range fails loud, nothing started. */
    pl_set(0, PLAYLIST_REPEAT_ALL, true); pipe_reset();
    player_stop();
    assert(player_play(0) == ESP_ERR_INVALID_ARG);
    assert(g_play == 0 && state() == PLAYER_STOPPED);

    /* pipeline rejects the play (e.g. queue full): surfaced, left STOPPED (we did stop). */
    pl_set(3, PLAYLIST_REPEAT_ALL, true); pipe_reset();
    g_play_ret = ESP_FAIL;
    assert(player_play(0) == ESP_FAIL);
    assert(state() == PLAYER_STOPPED);

    /* decode/open error (async, via the pipeline): stop. */
    pl_set(3, PLAYLIST_REPEAT_ALL, true); pipe_reset();
    assert(player_play(0) == ESP_OK);
    g_end_cb(PIPE_END_ERROR);
    assert(state() == PLAYER_STOPPED);

    printf("player: fail-loud on not-ready / empty / pipeline error / decode error OK\n");
}

static void test_bt_safety(void)
{
    pl_set(3, PLAYLIST_REPEAT_ALL, true); pipe_reset();
    player_init();
    assert(player_set_output(VOLUME_OUT_BT) == ESP_OK);
    assert(g_vol_out == VOLUME_OUT_BT);             /* volume routing kept consistent */

    /* speaker has not acknowledged the volume: the play succeeds but the stream is HELD
       (never blast at the speaker's default volume) -- nothing reaches the pipeline yet. */
    g_bt_acked = false;
    assert(player_play(0) == ESP_OK);
    assert(g_play == 0 && state() == PLAYER_STOPPED);

    /* ack arrives: the maintenance poll completes the deferred start. */
    g_bt_acked = true;
    player_poll();
    assert(g_play == 1 && state() == PLAYER_PLAYING);

    /* auto-advance is gated too: EOF with the ack lost defers rather than blasting... */
    pipe_reset(); g_bt_acked = false;
    g_end_cb(PIPE_END_EOF);
    assert(state() == PLAYER_STOPPED && g_play == 0);

    /* ...and the poll picks the held track up once the speaker confirms. */
    g_bt_acked = true;
    player_poll();
    assert(state() == PLAYER_PLAYING && g_play == 1);

    player_set_output(VOLUME_OUT_DAC);
    player_stop();
    printf("player: BT volume-ack gate holds streaming until confirmed OK\n");
}

static void test_unsupported_format(void)
{
    /* The sink refused the track's format (non-44.1 kHz over BT): the player must stop AND
       flag it, so the UI can say "not playable" instead of an unexplained silent stop. */
    pl_set(3, PLAYLIST_REPEAT_ALL, true); pipe_reset();
    player_init();
    assert(player_play(0) == ESP_OK);
    g_end_cb(PIPE_END_UNSUPPORTED);
    player_status_t s;
    assert(player_get_state(&s) == ESP_OK);
    assert(s.state == PLAYER_STOPPED && s.track_unsupported);

    /* A fresh play attempt clears the notice. */
    assert(player_play(1) == ESP_OK);
    assert(player_get_state(&s) == ESP_OK && !s.track_unsupported);
    printf("player: unsupported-format stop is flagged and cleared on retry OK\n");
}

static void test_bt_drop_fallback(void)
{
    /* The BT link dies while it is the active output: the poll must stop playback (it now
       streams into a dead sink) and reroute to the DAC so the UI is not stuck on BT. */
    pl_set(3, PLAYLIST_REPEAT_ALL, true); pipe_reset();
    player_init();
    g_bt_connected = true; g_bt_acked = true;
    assert(player_set_output(VOLUME_OUT_BT) == ESP_OK);
    assert(player_play(0) == ESP_OK && state() == PLAYER_PLAYING);

    g_bt_connected = false;
    player_poll();
    assert(state() == PLAYER_STOPPED);
    assert(g_vol_out == VOLUME_OUT_DAC);
    printf("player: BT link drop stops playback and falls back to the DAC OK\n");
}

int main(void)
{
    test_play_then_playing();
    test_auto_advance_on_eof();
    test_next_prev();
    test_repeat_off_end();
    test_repeat_one();
    test_pause_resume();
    test_errors();
    test_bt_safety();
    test_unsupported_format();
    test_bt_drop_fallback();
    printf("player: all assertions passed\n");
    return 0;
}
