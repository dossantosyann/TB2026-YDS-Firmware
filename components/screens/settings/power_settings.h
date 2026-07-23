/**
 * @file power_settings.h
 * @brief Power sub-menu (reached from Settings): sleep delay, auto power-off, power off.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Power sub-menu, ready to push onto the navigator.
 * @return Pointer to the Power sub-menu screen.
 */
screen_t *power_settings_screen(void);
