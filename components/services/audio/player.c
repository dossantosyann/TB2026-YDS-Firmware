/**
 * @file player.c
 * @brief Playback transport state machine over the playlist and pipeline.
 * @ingroup services_audio_player
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "player.h"
#include "pipeline.h"
#include "storage.h"
#include "sink_i2s_dac.h"
#include "sink_bluetooth.h"
#include "bluetooth.h"
#include "settings.h"
#include "esp_log.h"

static const char *TAG = "player";

static player_state_t  s_state  = PLAYER_STOPPED;
static volume_output_t s_output = VOLUME_OUT_DAC;
static bool            s_unsupported;  /* last stop was a sink format refusal (see player.h) */

/* A Bluetooth (re)start held until the speaker confirms the volume we pushed (never blast).
   player_poll(), run from the maintenance loop, completes it once bluetooth_volume_acked(). */
static volatile bool s_bt_pending;

/* A live output switch to Bluetooth held until the speaker confirms the volume (never blast).
   Unlike s_bt_pending the stream is NOT stopped: the previous output keeps playing while we
   wait, and player_poll() performs the seamless sink switch on the ack. */
static volatile bool s_switch_pending;

/* True while the active output is Bluetooth and the speaker has not confirmed the volume:
   streaming then would risk a blast, so the stream is held (not refused) until the ack lands. */
static bool bt_volume_unsafe(void)
{
    return s_output == VOLUME_OUT_BT && !bluetooth_volume_acked();
}

/* Start the playlist's current track. The caller has already stopped any previous stream, so
   every failure path here leaves the transport STOPPED (no orphaned "playing" state). */
static esp_err_t start_current(void)
{
    if (bt_volume_unsafe()) {
        /* Hold the stream until the speaker acks the volume we just primed; player_poll()
           resumes it on the ack. Returning OK: the switch/play succeeded, sound is imminent. */
        s_bt_pending = true;
        s_state = PLAYER_STOPPED;
        ESP_LOGI(TAG, "BT volume pending; stream deferred until ack");
        return ESP_OK;
    }
    s_bt_pending  = false;
    s_unsupported = false;             /* a fresh attempt clears the "not playable" notice */
    playlist_track_t t;
    esp_err_t err = playlist_current(&t);
    if (err == ESP_OK) err = pipeline_play_file(t.path);
    s_state = (err == ESP_OK) ? PLAYER_PLAYING : PLAYER_STOPPED;
    return err;
}

/* End-of-track notification from the pipeline (audio task context): advance and keep going. */
static void on_track_end(pipeline_end_reason_t reason)
{
    if (reason == PIPE_END_UNSUPPORTED) {
        /* The sink refused the format (non-44.1 kHz over Bluetooth): stop and flag it so the
           UI can say the track is not playable, instead of a silent unexplained stop. */
        ESP_LOGW(TAG, "track format not supported on this output; stopping");
        s_unsupported = true;
        s_state = PLAYER_STOPPED;
        return;
    }
    if (reason == PIPE_END_ERROR) {
        ESP_LOGE(TAG, "playback error; stopping");
        s_state = PLAYER_STOPPED;
        return;
    }

    /* EOF: repeat-one replays the same track, otherwise advance (playlist applies wrap). */
    playlist_track_t t;
    esp_err_t err = (playlist_get_repeat() == PLAYLIST_REPEAT_ONE)
                        ? playlist_current(&t)
                        : playlist_next(&t);
    if (err != ESP_OK) {                      /* end of list with repeat off */
        s_state = PLAYER_STOPPED;
        return;
    }
    if (bt_volume_unsafe()) {           /* volume un-acked (e.g. a knob turn): defer, don't stop */
        s_bt_pending = true;
        s_state = PLAYER_STOPPED;
    } else if (pipeline_play_file(t.path) != ESP_OK) {
        s_state = PLAYER_STOPPED;
    }
}

esp_err_t player_init(void)
{
    pipeline_set_track_end_cb(on_track_end);
    /* Route the volume service's Bluetooth output to the AVRCP setter. The player is the only
       service that links the BT stack, so the wiring lives here (not in volume.c, which stays
       BT-agnostic) and selecting VOLUME_OUT_BT then actually drives the speaker volume. */
    volume_set_bt_handler(bluetooth_set_absolute_volume);
    return ESP_OK;
}

esp_err_t player_set_output(volume_output_t out)
{
    const audio_sink_t *sink;
    switch (out) {
    case VOLUME_OUT_DAC:  sink = sink_i2s_dac_get();   break;
    case VOLUME_OUT_BT:   sink = sink_bluetooth_get(); break;
    case VOLUME_OUT_NONE: sink = NULL;                 break;  /* software mute: no jack-detect HW */
    default:              return ESP_ERR_INVALID_ARG;
    }

    /* A Bluetooth switch needs a speaker to route to; refuse only when none is connected. We do
       NOT refuse merely because the volume is not acked yet: routing to BT re-applies the knob
       (volume_set_output below), which primes the speaker's volume, and start_current() then
       holds the actual stream until the ack lands (never blast). Checking the ack before priming
       would deadlock -- the ack can only arrive after we have sent a volume. */
    if (out == VOLUME_OUT_BT && !bluetooth_is_connected()) {
        ESP_LOGW(TAG, "no Bluetooth speaker connected; output unchanged");
        return ESP_ERR_INVALID_STATE;
    }
    if (out == s_output) return ESP_OK;   /* already on this output: don't disturb playback */

    if (out != VOLUME_OUT_BT) {           /* leaving BT: drop any deferred start/switch */
        s_bt_pending     = false;
        s_switch_pending = false;
    }
    s_output = out;
    pipeline_set_sink(sink);              /* NULL (mute) is ignored; the stream is stopped below */
    volume_set_output(out);               /* primes the speaker's volume when routing to BT */

    /* PAUSED/STOPPED only remember the choice for the next playback. */
    if (s_state != PLAYER_PLAYING) return ESP_OK;

    /* Live re-route, keeping the decode position so the track continues where it was. */
    if (out == VOLUME_OUT_NONE) {         /* software mute: no sink to route to, stop the stream */
        pipeline_stop();
        s_state = PLAYER_STOPPED;
        return ESP_OK;
    }
    if (bt_volume_unsafe()) {
        /* Hold the switch until the speaker acks the volume (never blast); the current output
           keeps playing meanwhile and player_poll() performs the switch on the ack. */
        s_switch_pending = true;
        return ESP_OK;
    }
    return pipeline_switch_sink();
}

esp_err_t player_play(size_t index)
{
    if (!storage_ready()) return ESP_ERR_INVALID_STATE;

    /* Validate the selection before disturbing playback: a bad index leaves the current
       stream untouched. Only once it is valid do we stop the old stream and open the new. */
    playlist_track_t t;
    esp_err_t err = playlist_select(index, &t);
    if (err != ESP_OK) return err;
    pipeline_stop();                          /* one decoder stream at a time */
    return start_current();
}

esp_err_t player_start(void)
{
    if (s_state != PLAYER_STOPPED) return ESP_ERR_INVALID_STATE;
    if (!storage_ready())          return ESP_ERR_INVALID_STATE;

    /* Fresh shuffle picks a random first track; ascending order keeps the current position. */
    if (playlist_get_shuffle()) {
        esp_err_t err = playlist_random(NULL);
        if (err != ESP_OK) return err;
    }
    pipeline_stop();
    return start_current();
}

esp_err_t player_pause(void)
{
    if (s_state != PLAYER_PLAYING) return ESP_ERR_INVALID_STATE;
    esp_err_t err = pipeline_pause();
    if (err == ESP_OK) s_state = PLAYER_PAUSED;
    return err;
}

esp_err_t player_resume(void)
{
    if (s_state != PLAYER_PAUSED) return ESP_ERR_INVALID_STATE;
    esp_err_t err = pipeline_resume();
    if (err == ESP_OK) s_state = PLAYER_PLAYING;
    return err;
}

esp_err_t player_stop(void)
{
    s_bt_pending     = false;
    s_switch_pending = false;
    s_unsupported = false;
    pipeline_stop();
    s_state = PLAYER_STOPPED;
    return ESP_OK;
}

void player_poll(void)
{
    /* Bluetooth link gone while it is the active output: stop the stream (it plays into a dead
       sink, silently) and fall back to the wired DAC so the UI is not stuck routed to Bluetooth.
       Polled here because the BT service has no disconnect callback; 100 ms latency is fine. */
    if (s_output == VOLUME_OUT_BT && !bluetooth_is_connected()) {
        ESP_LOGW(TAG, "Bluetooth link lost; falling back to wired output");
        s_bt_pending     = false;
        s_switch_pending = false;
        pipeline_stop();
        s_state = PLAYER_STOPPED;
        player_set_output(VOLUME_OUT_DAC);
        return;
    }

    /* Complete a live output switch held for the volume ack (see player_set_output). If the
       stream ended while we waited, drop it -- s_bt_pending drives the (re)start instead. */
    if (s_switch_pending) {
        if (s_state != PLAYER_PLAYING) {
            s_switch_pending = false;
        } else if (bluetooth_volume_acked()) {
            s_switch_pending = false;
            pipeline_switch_sink();
        }
    }

    /* Complete a Bluetooth start that was held for the volume ack (see start_current). Runs on
       the maintenance loop, right after volume_poll() has had the chance to push the volume. */
    if (s_bt_pending && bluetooth_volume_acked()) start_current();
}

esp_err_t player_next(void)
{
    if (s_state == PLAYER_STOPPED) return ESP_ERR_INVALID_STATE;
    pipeline_stop();
    playlist_track_t t;
    esp_err_t err = playlist_next(&t);
    if (err != ESP_OK) { s_state = PLAYER_STOPPED; return err; }
    return start_current();
}

esp_err_t player_prev(void)
{
    if (s_state == PLAYER_STOPPED) return ESP_ERR_INVALID_STATE;
    pipeline_stop();
    playlist_track_t t;
    esp_err_t err = playlist_prev(&t);
    if (err != ESP_OK) { s_state = PLAYER_STOPPED; return err; }
    return start_current();
}

esp_err_t player_get_state(player_status_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    out->state = s_state;
    if (s_state == PLAYER_STOPPED || playlist_current(&out->track) != ESP_OK) {
        out->track = (playlist_track_t){0};
    }
    pipeline_get_position(&out->elapsed_ms, &out->total_ms);
    out->track_unsupported = s_unsupported;
    return ESP_OK;
}

void player_save_resume(void)
{
    /* Persist the current track so Now Playing re-selects it after any power-off (app.c reads
       "last_path" at boot). Save on both playing and paused -- only STOPPED has no track, and
       there we leave the previous value untouched. "last_pos" is stored for a future mid-track
       resume; playback restarts at 0:00 until the decoder gains seek support. */
    player_status_t st;
    if (player_get_state(&st) != ESP_OK || st.state == PLAYER_STOPPED) return;
    settings_set_str("last_path", st.track.path);
    settings_set_u32("last_pos", st.elapsed_ms);
}
