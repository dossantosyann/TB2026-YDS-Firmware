/**
 * @file power.h
 * @brief Power service: battery state, charge detection, and board power on/off.
 *
 * Owns the energy domain of the board:
 *   - EnableReg self-hold (PIN_REG_EN): keep the regulator latched on at boot, and
 *     release it on power_shutdown().
 *   - battery snapshot from the MAX17260 fuel gauge (state of charge, voltage,
 *     current) plus a derived charge/level state for the UI.
 *   - low-battery policy: a graceful auto-shutdown when the cell is critically low
 *     and not being fed.
 *   - USB-C data/charge mux routing (TC7USB40MU via PIN_USB_DIR).
 *
 * This service does not own a task. The maintenance task calls power_tick()
 * periodically (see services/maintenance). The fuel gauge and GPIO expander
 * drivers must already be initialised before power_tick() runs.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @defgroup services_power Power service
 * @ingroup services
 * @brief Battery state, charge detection, low-battery policy, and power on/off.
 * @{
 */

/** @brief Battery charge level band, derived from state of charge. */
typedef enum {
    POWER_LEVEL_NORMAL = 0,  ///< above the low threshold
    POWER_LEVEL_LOW,         ///< at or below the low threshold (warn the user)
    POWER_LEVEL_CRITICAL,    ///< at or below the critical threshold (graceful shutdown)
} power_level_t;

/** @brief USB-C mux routing through the TC7USB40MU (PIN_USB_DIR). */
typedef enum {
    POWER_USB_DATA = 0,  ///< LOW: route D+/D- to the CP2102N (console / flash)
    POWER_USB_CHARGE,    ///< HIGH: route to the MAX77757 charger (USB source current detection)
} power_usb_route_t;

/** @brief Callback run by power_shutdown() before the rail is cut (e.g. amp+DAC power-down). */
typedef void (*power_shutdown_hook_t)(void);

/** @brief Cached battery/charge snapshot, refreshed by power_tick(). */
typedef struct {
    bool          valid;          ///< false until the first successful fuel-gauge read
    float         soc_pct;        ///< state of charge, %
    float         voltage_v;      ///< cell voltage, V
    float         current_ma;     ///< + into the cell (charge), - out (discharge), mA
    bool          charging;       ///< current into the cell above the charge threshold
    bool          external_power; ///< input present (INOKB asserted): USB/charger plugged in
    power_level_t level;          ///< charge band derived from soc_pct
} power_state_t;

/**
 * @brief Latch the regulator on by driving EnableReg high.
 *
 * The rail is only held by USB / the power button at boot; the firmware must take
 * over the hold immediately or releasing them cuts power. Call this first thing at
 * startup, before any other bring-up. Preloads the output high before enabling the
 * driver so the pin never drives low on its way up.
 */
void power_self_hold(void);

/**
 * @brief Refresh the cached battery state and apply the low-battery policy.
 *
 * Reads the fuel gauge and the INOKB input, updates the snapshot, and — if the cell
 * is critically low while discharging on battery — calls power_shutdown(). A failed
 * gauge read marks the snapshot invalid and takes no action (fail-safe). Intended to
 * be called from the maintenance task, not at interrupt time.
 */
void power_tick(void);

/**
 * @brief Copy the latest cached battery snapshot.
 *
 * @param[out] out  Receives the snapshot. Single writer (maintenance task), so a
 *                  concurrent read can at worst see one stale field — cosmetic for the UI.
 */
void power_get_state(power_state_t *out);

/**
 * @brief Route the USB-C mux to data or charge.
 *
 * Configures PIN_USB_DIR as an output and drives it. Glitch-free: sets the level
 * before enabling the driver.
 */
void power_set_usb_route(power_usb_route_t route);

/**
 * @brief Boot-time USB mux hand-off: charger first, then console.
 *
 * Routes the USB-C data lines to the MAX77757 (POWER_USB_CHARGE) so the charger can
 * run its source-current detection, then spawns a short-lived task that hands the
 * lines to the CP2102N (POWER_USB_DATA) as soon as INOKB asserts (input detected),
 * or after a 10 s safety timeout if it never does. The task self-deletes when done.
 * Call once, early in boot. Requires gpio_expander_init() to have run (reads INOKB).
 */
void power_usb_autoroute_start(void);

/**
 * @brief Register a callback to run at the start of power_shutdown().
 *
 * Lets a subsystem clean up before the rail drops without power having to know the
 * details — e.g. the audio service registers an amp-off-then-DAC-off sequence here,
 * keeping the mute/shutdown ordering in the audio service. Pass NULL to clear.
 */
void power_set_shutdown_hook(power_shutdown_hook_t hook);

/**
 * @brief Run the shutdown hook (if any), release EnableReg, and power the board off.
 *
 * Drives PIN_REG_EN low; the rail collapses within the regulator's turn-off time and
 * this never returns. If USB is still latching the regulator on, power stays up.
 */
void power_shutdown(void) __attribute__((noreturn));

/** @} */
