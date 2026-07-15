/**
 * @file battery_test.h
 * @brief Battery autonomy test screen (reached with A from the Stats > Battery page).
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton battery-autonomy-test screen, ready to push onto the navigator.
 * @return Pointer to the battery test screen.
 */
screen_t *battery_test_screen(void);
