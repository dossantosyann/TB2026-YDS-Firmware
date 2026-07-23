/**
 * @file output_select.h
 * @brief Output-selection popup (reached from Now Playing, UP).
 *
 * A small centered popup to route audio to the wired Jack or a connected Bluetooth speaker.
 * The Bluetooth entry is dimmed and inert while no device is connected. A third entry jumps
 * straight to the Bluetooth settings screen for quick access to pairing/connection.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton output-selection popup, ready to push onto the navigator.
 * @return Pointer to the output-selection screen.
 */
screen_t *output_select_screen(void);
