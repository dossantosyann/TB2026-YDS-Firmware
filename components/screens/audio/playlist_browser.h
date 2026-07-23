/**
 * @file playlist_browser.h
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Playlist Browser screen.
 *
 * Shows the play queue from the current track onward (current pinned at the top,
 * upcoming tracks below). UP/DOWN navigate; SELECT on an upcoming track opens a
 * move-up / move-down / remove menu. Reached from Now Playing via DOWN; BACK, or
 * UP while at the top, returns to Now Playing.
 */
screen_t *playlist_browser_screen(void);
