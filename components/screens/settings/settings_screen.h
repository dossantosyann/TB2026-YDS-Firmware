/**
 * @file settings_screen.h
 * @brief The Settings menu (top-level), reached from the home screen.
 *
 * A vertical list over the four sub-screens: Bluetooth, Audio, Screen and Power.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Settings screen, ready to push onto the navigator.
 * @return Pointer to the Settings screen.
 */
screen_t *settings_screen(void);
