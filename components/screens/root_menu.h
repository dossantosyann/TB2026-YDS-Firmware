/**
 * @file root_menu.h
 * @brief The home screen shown after boot: a vertical list of icon+label entries.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton root-menu screen, ready to push onto the navigator.
 * @return Pointer to the root-menu screen.
 */
screen_t *root_menu(void);
