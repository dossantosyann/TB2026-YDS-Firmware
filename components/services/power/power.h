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
 * This service owns no long-lived task: the maintenance task calls power_tick() periodically
 * (see services/maintenance), and the only task it ever spawns is the short-lived USB mux
 * hand-off, which self-deletes. The fuel gauge and GPIO expander drivers must already be
 * initialised before power_tick() runs.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

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
    float         tte_s;          ///< time to empty, s (only meaningful while discharging; 0 otherwise)
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
 * @brief The route currently driven onto PIN_USB_DIR (for diagnostics).
 *
 * Reports POWER_USB_DATA before the first power_set_usb_route() call, when the pin is not
 * driven yet — boot calls it immediately, so only the very first instants of bring-up lie.
 */
power_usb_route_t power_get_usb_route(void);

/**
 * @brief USB mux hand-off: charger first, then console.
 *
 * Routes the USB-C data lines to the MAX77757 (POWER_USB_CHARGE) so the charger can run its
 * BC1.2 source detection, then spawns a short-lived task that hands the lines to the CP2102N
 * (POWER_USB_DATA) once INOKB asserts (valid input) and the detection has had time to finish,
 * or after a safety timeout if no source ever shows up. The task self-deletes when done.
 *
 * Call once early in boot; power_tick() then re-arms it on every plug event, since BC1.2 needs
 * the data lines and would otherwise leave a legacy charger capped at its 500 mA default. Calls
 * made while a hand-off is already running are ignored. Requires gpio_expander_init() (INOKB).
 *
 * The console loses its USB device for the duration of each hand-off.
 */
void power_usb_autoroute_start(void);

/**
 * @brief Enable or disable the built-in critical-battery auto-shutdown (default enabled).
 *
 * The autonomy test disables it while a run is in progress so the service can detect the
 * critical level itself, write its final log, and then call power_shutdown() in order. Restore
 * it (true) when the run ends or is cancelled.
 *
 * @param enable  true to keep the built-in graceful shutdown, false to suspend it.
 */
void power_set_low_batt_shutdown(bool enable);

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
 * Persists a "clean shutdown" marker in NVS just before the rail drops, so the next
 * boot can tell an intentional off from a crash or hard power loss (the EnableReg
 * scheme makes every reset look like ESP_RST_POWERON). @see power_last_off_cause
 */
void power_shutdown(void) __attribute__((noreturn));

/** @brief How the previous session ended, reconstructed at boot. */
typedef enum {
    POWER_OFF_POWER_LOSS = 0,  ///< no marker, no core dump: the rail died without warning
                               ///< (battery connector, supervisor, brownout — or first boot)
    POWER_OFF_CLEAN,           ///< power_shutdown() ran (user off, critical battery, autonomy end)
    POWER_OFF_CRASH,           ///< a panic core dump was found in flash
} power_off_cause_t;

/**
 * @brief Classify how the previous session ended (call once at boot, after NVS is up).
 *
 * Consumes (reads + erases) the clean-shutdown marker power_shutdown() left, and checks
 * the coredump flash partition for a panic dump. Erasing the marker here keeps a stale
 * flag from masking the next crash. The core dump itself is NOT erased here: the boot
 * sequence exports it to the SD card first, then erases it (see app.c).
 */
void power_boot_off_check(void);

/**
 * @brief How the previous session ended. Valid after power_boot_off_check().
 */
power_off_cause_t power_last_off_cause(void);

/** @brief Faulting task name when the cause is POWER_OFF_CRASH ("" otherwise). */
const char *power_last_crash_task(void);

/** @brief Faulting program counter when the cause is POWER_OFF_CRASH (0 otherwise). */
uint32_t power_last_crash_pc(void);

/**
 * @name Power-saving timeouts (user preferences, persisted in NVS)
 *
 * Idle durations, in milliseconds, driving the two power-saving policies. Both are
 * cached on first read (no flash access on the hot paths that poll them) and 0 means
 * the policy is disabled ("Never"). Setters persist the new value immediately.
 * @{
 */

/** @brief Idle time before the screen blanks (UI task). 0 = never sleep. */
uint32_t power_get_sleep_ms(void);
/** @brief Set and persist the screen-sleep timeout. @see power_get_sleep_ms */
void     power_set_sleep_ms(uint32_t ms);

/** @brief Idle time before the board auto powers off (maintenance task). 0 = never. */
uint32_t power_get_poweroff_ms(void);
/** @brief Set and persist the auto power-off timeout. @see power_get_poweroff_ms */
void     power_set_poweroff_ms(uint32_t ms);

/** @} */

/** @} */
