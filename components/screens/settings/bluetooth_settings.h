/**
 * @file bluetooth_settings.h
 * @brief Bluetooth settings sub-screen (reached from Settings).
 *
 * One combined screen: the paired (known) devices on top, then a scan action and the live nearby
 * results with signal strength. Connect from either list; a popup on a paired device offers
 * Connect/Disconnect and Forget. The radio is kept off by default and only powered while scanning
 * or connected.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Bluetooth settings screen, ready to push onto the navigator.
 * @return Pointer to the Bluetooth settings screen.
 */
screen_t *bluetooth_settings_screen(void);
