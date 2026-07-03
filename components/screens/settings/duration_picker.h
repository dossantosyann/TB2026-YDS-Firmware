/**
 * @file duration_picker.h
 * @brief Reusable settings screen: pick one idle-timeout value from a preset list.
 *
 * A vertical list of predefined durations (with a "Never" entry for the value 0). UP/DOWN
 * move the highlight; the choice is applied and persisted through the caller-supplied
 * setter on BACK, only if it actually moved. Two instances back the Power sub-menu
 * (screen-sleep delay and auto power-off delay); each owns its own state via a
 * duration_picker_t that embeds screen_t as its first member.
 */
#pragma once

#include "screen.h"
#include <stdint.h>

/** @brief A duration picker instance. Embed-and-cast pattern: base must stay first. */
typedef struct {
    screen_t        base;       /**< Screen vtable; must stay first for the cast. */
    const char     *title;      /**< Header label (e.g. "SLEEP DELAY"). */
    const uint32_t *presets;    /**< Preset values in ms; 0 renders as "Never". */
    int             count;      /**< Number of presets. */
    int             sel;        /**< Highlighted index (runtime state). */
    uint32_t      (*get)(void); /**< Load the current value (to seed the highlight). */
    void          (*set)(uint32_t ms); /**< Persist the chosen value. */
} duration_picker_t;

/**
 * @brief Initialise a duration picker's vtable and configuration.
 *
 * @param dp       Picker to initialise (storage owned by the caller, must outlive use).
 * @param title    Header text.
 * @param presets  Preset array in ms (must outlive the picker; not copied). 0 = "Never".
 * @param count    Number of presets.
 * @param get      Getter for the current value (seeds the highlight on entry).
 * @param set      Setter to persist the chosen value on exit.
 */
void duration_picker_init(duration_picker_t *dp, const char *title,
                          const uint32_t *presets, int count,
                          uint32_t (*get)(void), void (*set)(uint32_t ms));
