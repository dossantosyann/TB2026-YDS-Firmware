/**
 * @file power_settings.h
 * @brief Power sub-menu (reached from Settings): sleep delay, auto power-off, power off.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Power sub-menu, ready to push onto the navigator.
 * @return Pointer to the Power sub-menu screen.
 */
screen_t *power_settings_screen(void);
