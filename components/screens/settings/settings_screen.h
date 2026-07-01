/**
 * @file settings_screen.h
 * @brief The Settings menu (top-level), reached from the home screen.
 *
 * A vertical list with three sub-screens (Bluetooth, Audio, Screen), each a stub
 * for now.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Settings screen, ready to push onto the navigator.
 * @return Pointer to the Settings screen.
 */
screen_t *settings_screen(void);
