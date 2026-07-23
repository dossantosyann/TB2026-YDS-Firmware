/**
 * @file stats_screen.h
 * @brief The Statistics menu (top-level), reached from the home screen: five live
 *        diagnostics pages (battery, storage, inputs, peripherals, system).
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Statistics screen, ready to push onto the navigator.
 * @return Pointer to the Statistics screen.
 */
screen_t *stats_screen(void);
