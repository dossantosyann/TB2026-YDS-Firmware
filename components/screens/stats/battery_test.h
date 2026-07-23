/**
 * @file battery_test.h
 * @brief Battery autonomy test screen (reached with DOWN from the Stats > Battery page).
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton battery-autonomy-test screen, ready to push onto the navigator.
 * @return Pointer to the battery test screen.
 */
screen_t *battery_test_screen(void);
