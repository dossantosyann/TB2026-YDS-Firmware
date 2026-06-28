/**
 * @file input.h
 * @brief Input service: the six buttons -> one semantic ui_event_t in a single queue.
 *
 * Interrupt-driven, strictly discrete: one press emits exactly one @ref ui_event_t (no long-press,
 * no auto-repeat, no combos). Five buttons hang off the PI4IOE5V9554A expander (one shared INT on
 * GPIO35); button A has its own GPIO39 line. Each ISR only notifies the input task, which reads the
 * state over I2C / GPIO (never in ISR context), debounces, and posts the event. The task sleeps
 * between interrupts, so an idle device costs no I2C traffic (see Rule 13). The UI layer drains the
 * queue with input_get_event(); the debounce + mapping core lives in @ref services_input_logic.
 *
 * Wake provisioning: A (GPIO39) is an RTC-IO and is the intended deep-sleep wake source. This
 * service only configures the pin wake-compatibly and exposes @ref INPUT_WAKE_GPIO /
 * @ref INPUT_WAKE_ACTIVE_LEVEL; it never calls esp_sleep_* — arming is the future sleep service's job.
 */
#pragma once

#include "esp_err.h"
#include "event.h"
#include "board_pins.h"
#include "freertos/FreeRTOS.h"

/**
 * @defgroup services_input Input service
 * @ingroup services
 * @brief Buttons -> ui_event_t queue, interrupt-driven and discrete (one press, one event).
 * @{
 */

/** @name Deep-sleep wake provisioning for button A (reused as-is by a future sleep service) */
///@{
/** @brief GPIO to arm as the ext0 wake source. A (GPIO39) is input-only and an RTC-IO. */
#define INPUT_WAKE_GPIO         PIN_BTN_A
/** @brief Active level of A: A is active-HIGH (pressed pulls it high; external pull-down keeps
 *  it low at rest), unlike the active-low expander buttons. Wake on 1. */
#define INPUT_WAKE_ACTIVE_LEVEL 1
///@}

/**
 * @brief Bring up the input service: queue, both ISRs, the input task, and A's wake-ready pin.
 *
 * Creates the event queue, configures the expander INT (GPIO35, falling edge) and A (GPIO39,
 * input, no internal pull), installs the GPIO ISR service, registers both handlers, and starts
 * the input task. The expander itself must already be initialised by the bsp. Idempotent.
 *
 * @return ESP_OK; ESP_ERR_NO_MEM if the queue or task cannot be created; otherwise the GPIO error.
 */
esp_err_t input_init(void);

/**
 * @brief Block until the next input event (the UI task's read side of the queue).
 *
 * @param[out] out          Receives the next @ref ui_event_t.
 * @param ticks_to_wait     How long to wait (portMAX_DELAY to block indefinitely).
 * @return true if an event was dequeued, false on timeout.
 */
bool input_get_event(ui_event_t *out, TickType_t ticks_to_wait);

/** @} */
