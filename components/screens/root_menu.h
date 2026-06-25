/**
 * @file root_menu.h
 * @brief The home screen shown after boot: a centered row of icon tiles.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton root-menu screen, ready to push onto the navigator.
 * @return Pointer to the root-menu screen.
 */
screen_t *root_menu(void);
