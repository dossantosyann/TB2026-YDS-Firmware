#pragma once

#include "screen.h"

/**
 * @brief Get the singleton Now Playing screen.
 *
 * Main music screen: shows track metadata, transport controls, progress bar,
 * and volume level. Reached from the root menu; UP → output selector,
 * DOWN → playlist browser.
 */
screen_t *now_playing_screen(void);
