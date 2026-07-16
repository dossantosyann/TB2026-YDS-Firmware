/**
 * @file fuel_gauge_debug.h
 * @brief Hidden fuel-gauge debug screen (reached with UP from the Stats > Battery page).
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton fuel-gauge debug screen, ready to push onto the navigator.
 * @return Pointer to the fuel-gauge debug screen.
 */
screen_t *fuel_gauge_debug_screen(void);
