/**
 * @file player.c
 * @brief Playback transport state machine over the playlist and pipeline.
 * @ingroup services_audio_player
 */
#include "player.h"
#include "pipeline.h"
#include "storage.h"
#include "sink_i2s_dac.h"
#include "sink_bluetooth.h"
#include "bluetooth.h"
#include "esp_log.h"

static const char *TAG = "player";

static player_state_t  s_state  = PLAYER_STOPPED;
static volume_output_t s_output = VOLUME_OUT_DAC;

/* True while the active output is Bluetooth and the speaker has not confirmed the volume:
   streaming then would risk a blast, so playback is refused until the ack lands. */
static bool bt_volume_unsafe(void)
{
    return s_output == VOLUME_OUT_BT && !bluetooth_volume_acked();
}

/* Start the playlist's current track. The caller has already stopped any previous stream, so
   every failure path here leaves the transport STOPPED (no orphaned "playing" state). */
static esp_err_t start_current(void)
{
    if (bt_volume_unsafe()) {
        ESP_LOGW(TAG, "BT volume not acknowledged yet; not streaming");
        s_state = PLAYER_STOPPED;
        return ESP_ERR_INVALID_STATE;
    }
    playlist_track_t t;
    esp_err_t err = playlist_current(&t);
    if (err == ESP_OK) err = pipeline_play_file(t.path);
    s_state = (err == ESP_OK) ? PLAYER_PLAYING : PLAYER_STOPPED;
    return err;
}

/* End-of-track notification from the pipeline (audio task context): advance and keep going. */
static void on_track_end(pipeline_end_reason_t reason)
{
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
    if (bt_volume_unsafe() || pipeline_play_file(t.path) != ESP_OK) {
        s_state = PLAYER_STOPPED;
    }
}

esp_err_t player_init(void)
{
    pipeline_set_track_end_cb(on_track_end);
    return ESP_OK;
}

esp_err_t player_set_output(volume_output_t out)
{
    if (out == VOLUME_OUT_BT) {
        pipeline_set_sink(sink_bluetooth_get());
    } else if (out == VOLUME_OUT_DAC) {
        pipeline_set_sink(sink_i2s_dac_get());
    } else {
        return ESP_ERR_INVALID_ARG;
    }
    s_output = out;
    volume_set_output(out);
    return ESP_OK;
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
    pipeline_stop();
    s_state = PLAYER_STOPPED;
    return ESP_OK;
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
    return ESP_OK;
}
