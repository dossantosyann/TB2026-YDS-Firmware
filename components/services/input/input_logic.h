/**
 * @file input_logic.h
 * @brief Pure button-state -> UI-event logic: debounce + press-edge detection + mapping.
 *
 * The hardware-free core of the @ref services_input service, split out so it can be exercised
 * off-target (test/host/input_test.c). It takes a raw expander input byte, the A button level,
 * and a millisecond timestamp, and returns the UI events for buttons that were just pressed.
 * No I2C, no ISR, no RTC: feed it samples, get events. The service (input.c) supplies the real
 * samples from the IRQ-driven task.
 */
#pragma once

#include "event.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @defgroup services_input_logic Input logic (pure)
 * @ingroup services
 * @brief Debounce + press-edge detection + button->ui_event mapping, no hardware.
 * @{
 */

/** @brief Number of physical buttons handled (5 expander + A). */
#define INPUT_BTN_COUNT 6

/** @brief Leading-edge lockout in ms: a button's press is accepted only this long after the
 *  previous accepted press, which swallows contact bounce (must exceed the bounce duration). */
#define INPUT_DEBOUNCE_MS 30

/** @brief Debounce/edge state. Zero-initialise before the first call (memset or {0}). */
typedef struct {
    uint8_t  prev;                      /**< Pressed mask at the previous call (bit per button). */
    uint32_t last_ms[INPUT_BTN_COUNT];  /**< Timestamp of each button's last accepted press. */
    uint32_t count[INPUT_BTN_COUNT];    /**< Accepted (debounced) presses per button since reset. */
} input_debounce_t;

/**
 * @brief Fold one button sample into the debounce state, emitting just-pressed UI events.
 *
 * Detects the rising edge (released -> pressed) of each button and, if it clears the
 * @ref INPUT_DEBOUNCE_MS lockout, appends that button's @ref ui_event_t to @p out. Releases and
 * holds emit nothing; the expander's IO7 (charger INOKB) is never a button and is ignored.
 *
 * @param st         Debounce state, carried across calls (zero-initialised on first use).
 * @param exp_port   Raw expander input port byte (bit n = channel n; buttons are active-low).
 * @param a_pressed  A button pressed (true when GPIO39 reads high — A is active-high).
 * @param now_ms     Monotonic timestamp in ms (e.g. esp_timer_get_time()/1000).
 * @param[out] out   Receives the events for newly pressed buttons.
 * @param out_cap    Capacity of @p out (use @ref INPUT_BTN_COUNT to never truncate).
 * @return Number of events written to @p out (0..out_cap).
 */
int input_logic_update(input_debounce_t *st, uint8_t exp_port, bool a_pressed,
                       uint32_t now_ms, ui_event_t *out, int out_cap);

/** @} */
