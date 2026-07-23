/**
 * @file status_bar.h
 * @brief Global status overlay drawn on top of every screen.
 */
#pragma once

/**
 * @defgroup ui_status_bar Status bar
 * @ingroup ui
 * @brief Permanent top bar shared by every screen: Bluetooth state, active output,
 *        battery percent.
 * @{
 */

/** @brief Height in pixels of the reserved top bar. Screens render below this. */
#define STATUS_BAR_H 16

/**
 * @brief Paint the top status bar over the framebuffer.
 *
 * Called by the navigator right after the active screen renders (and once for the
 * initial paint), so the bar appears on every screen regardless of which one is on
 * top. It owns the top @ref STATUS_BAR_H rows: it clears that strip to black, then
 * draws the Bluetooth antenna (left), the active output (centre) and the battery
 * percentage (right). Screens are expected to keep their
 * content below @ref STATUS_BAR_H so nothing is hidden. Reads the cached snapshot
 * from the power service; shows "--%" until the fuel gauge has produced its first
 * reading.
 */
void status_bar_draw(void);

/** @} */
