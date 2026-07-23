/**
 * @file screen_settings.h
 * @brief Screen (display) settings sub-screen (reached from Settings): brightness
 *        slider and auto-sleep delay.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Screen settings screen, ready to push onto the navigator.
 * @return Pointer to the Screen settings screen.
 */
screen_t *screen_settings_screen(void);

/**
 * @brief Restore the saved brightness from NVS and apply it to the panel.
 *
 * Call once at boot, after display_oled_init() (the panel must be up). Falls back
 * to full brightness when nothing is stored yet.
 */
void screen_settings_restore(void);
