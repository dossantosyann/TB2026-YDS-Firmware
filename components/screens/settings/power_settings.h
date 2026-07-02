/**
 * @file power_settings.h
 * @brief Power-off sub-screen (reached from Settings): confirm, then cut the rail.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Power-off screen, ready to push onto the navigator.
 * @return Pointer to the Power-off screen.
 */
screen_t *power_settings_screen(void);
