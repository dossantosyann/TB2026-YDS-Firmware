/**
 * @file decoder_backend.h
 * @brief Internal contract between the decoder facade (decoder.c) and a format backend.
 *
 * Not a public header: only decoder.c and the per-format backends include it. The facade
 * owns the FILE* (it opens it, sniffs the format, and closes it); a backend keeps its own
 * decoder state and reads PCM from the file handed to open().
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "decoder.h"
#include <stdio.h>

/** @brief One format backend (MP3 or WAV). Single-stream: one open at a time. */
typedef struct {
    /** Read the header from @p f (positioned at byte 0) and fill @p fmt; init backend state. */
    esp_err_t (*open)(FILE *f, decoder_format_t *fmt);
    /** Decode the next chunk into @p pcm; @p *got_bytes is 0 at end of file. */
    esp_err_t (*read)(void *pcm, size_t cap_bytes, size_t *got_bytes);
    /** Free backend state. Does not close the FILE* (the facade owns it). */
    void (*close)(void);
} decoder_backend_t;

extern const decoder_backend_t decoder_mp3_backend;
extern const decoder_backend_t decoder_wav_backend;
