/**
 * @file audio_settings.h
 * @brief Audio settings sub-screen (reached from Settings): output routing and
 *        L/R balance slider.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Audio settings screen, ready to push onto the navigator.
 * @return Pointer to the Audio settings screen.
 */
screen_t *audio_settings_screen(void);
