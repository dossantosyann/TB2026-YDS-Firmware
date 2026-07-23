/**
 * @file storage_screen.h
 * @brief File browser over the SD card (top-level), reached from the home screen:
 *        navigate folders, play or delete a track.
 */
#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Storage screen, ready to push onto the navigator.
 * @return Pointer to the Storage screen.
 */
screen_t *storage_screen(void);
