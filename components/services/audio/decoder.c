/**
 * @file decoder.c
 * @brief Decoder facade: owns the FILE*, auto-detects the format, delegates to a backend.
 *
 * Detection is by extension (.mp3 / .wav), confirmed against the file's magic bytes; if the
 * two disagree the magic wins. Callers see only the concrete decoder.h API and never learn
 * which backend is active.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "decoder.h"
#include "decoder_backend.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */

static const char *TAG = "decoder";

static FILE                    *s_file;
static const decoder_backend_t *s_be;
static decoder_format_t         s_fmt;

static const char *ext_of(const char *path)
{
    const char *dot = strrchr(path, '.');
    return dot ? dot + 1 : "";
}

/* Sniff the first bytes to pick a backend; rewinds f to the start before returning. */
static const decoder_backend_t *detect(const char *path, FILE *f)
{
    uint8_t m[12] = {0};
    size_t  n = fread(m, 1, sizeof m, f);
    rewind(f);

    bool wav = n >= 12 && !memcmp(m, "RIFF", 4) && !memcmp(m + 8, "WAVE", 4);
    bool mp3 = n >= 3 && (!memcmp(m, "ID3", 3) ||
                          (m[0] == 0xFF && (m[1] & 0xE0) == 0xE0));

    const char *ext = ext_of(path);
    if (!strcasecmp(ext, "wav") && wav) return &decoder_wav_backend;
    if (!strcasecmp(ext, "mp3") && mp3) return &decoder_mp3_backend;

    /* Extension missing or lying: trust the magic bytes. */
    if (wav) return &decoder_wav_backend;
    if (mp3) return &decoder_mp3_backend;
    return NULL;
}

esp_err_t decoder_open(const char *path)
{
    if (s_be) return ESP_ERR_INVALID_STATE;

    FILE *f = fopen(path, "rb");
    if (!f) return ESP_ERR_NOT_FOUND;

    const decoder_backend_t *be = detect(path, f);
    if (!be) {
        fclose(f);
        ESP_LOGW(TAG, "unrecognised format: %s", path);
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_err_t err = be->open(f, &s_fmt);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "backend open failed (%s): %s", esp_err_to_name(err), path);
        be->close();
        fclose(f);
        return err;
    }

    s_file = f;
    s_be   = be;
    return ESP_OK;
}

esp_err_t decoder_format(decoder_format_t *fmt)
{
    if (!s_be) return ESP_ERR_INVALID_STATE;
    *fmt = s_fmt;
    return ESP_OK;
}

esp_err_t decoder_read(void *pcm, size_t cap_bytes, size_t *got_bytes)
{
    if (!s_be) return ESP_ERR_INVALID_STATE;
    return s_be->read(pcm, cap_bytes, got_bytes);
}

esp_err_t decoder_close(void)
{
    if (s_be)   { s_be->close(); s_be = NULL; }
    if (s_file) { fclose(s_file); s_file = NULL; }
    return ESP_OK;
}
