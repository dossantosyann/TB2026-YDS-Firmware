/**
 * @file bluetooth_settings.h
 * @brief Bluetooth settings sub-screen (reached from Settings). Stub for now.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Bluetooth settings screen, ready to push onto the navigator.
 * @return Pointer to the Bluetooth settings screen.
 */
screen_t *bluetooth_settings_screen(void);
