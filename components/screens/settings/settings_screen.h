/**
 * @file settings_screen.h
 * @brief The Settings menu (top-level), reached from the home screen.
 *
 * A vertical list over the four sub-screens: Bluetooth, Audio, Screen and Power.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Settings screen, ready to push onto the navigator.
 * @return Pointer to the Settings screen.
 */
screen_t *settings_screen(void);
