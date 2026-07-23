/**
 * @file volume.h
 * @brief Volume service: maps the pot to whichever output is active (DAC, Bluetooth, or none).
 *
 * Owns the volume potentiometer (ADC1, see @ref bsp_adc) and routes the knob level to the
 * selected output: the PCM5242 digital volume (with an L/R balance trim), the Bluetooth speaker
 * over AVRCP, or nowhere. It only writes when the mapped level changes, so a still knob costs no
 * I2C/AVRCP traffic, and it only ever touches the active output (power: no pointless writes to a
 * silent path). Each output has its own scale: the DAC uses a dB register byte, Bluetooth a
 * linear 0..0x7F (see @ref bsp_adc); switching output re-applies the current knob position.
 *
 * The Bluetooth setter is registered as a callback (volume_set_bt_handler) so this service does
 * not hard-link the Bluetooth stack when BT is unused.
 *
 * Not real-time: drive it from a periodic maintenance/UI loop via volume_poll(), not its own task.
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>

/** @brief Where volume_poll() sends the knob level. */
typedef enum {
    VOLUME_OUT_NONE = 0,  /**< No active output: sample the pot but send nothing. */
    VOLUME_OUT_DAC,       /**< Wired jack via the PCM5242 digital volume (default). */
    VOLUME_OUT_BT,        /**< Bluetooth speaker via AVRCP absolute volume. */
} volume_output_t;

/**
 * @defgroup services_audio_volume Volume service
 * @ingroup services
 * @brief Sole writer of the PCM5242 digital volume (pot -> DAC, with L/R balance).
 * @{
 */

/**
 * @brief Bring up the volume pot and push the current knob position to the DAC.
 *
 * Call once at startup, after audio_dac_init() (this service writes the DAC). Owns the ADC
 * unit handle internally. Idempotent.
 *
 * @return ESP_OK; otherwise the underlying ADC init error.
 */
esp_err_t volume_init(void);

/**
 * @brief Sample the pot and write the active output only if the mapped level changed.
 *
 * Call periodically (~10..20 Hz) from a maintenance/UI loop. The ADC read happens every
 * call (cheap; skipped under a fixed-volume override); the I2C/AVRCP write is skipped
 * when the level is unchanged (deadband).
 *
 * @param[out] out_mv     Optional: wiper voltage in millivolts (for display). May be NULL.
 * @param[out] out_level  Optional: the level applied to the active output (DAC byte or AVRCP
 *                        value, per the current output). May be NULL.
 * @return true if a new level was sent to the active output this call, false otherwise.
 */
bool volume_poll(int *out_mv, uint8_t *out_level);

/**
 * @brief Read the pot wiper voltage without touching any output (for diagnostics).
 *
 * Unlike volume_poll(), this never writes the DAC or AVRCP — it only samples the ADC. Brings up
 * the pot ADC on first call if volume_init() has not run yet, so the diagnostics screen can show
 * the knob voltage before the audio path is wired.
 *
 * @param[out] out_mv  Receives the wiper voltage in millivolts.
 * @return true on a successful read, false if the ADC is unavailable.
 */
bool volume_read_mv(int *out_mv);

/**
 * @brief Select which output volume_poll() drives. Default VOLUME_OUT_DAC.
 *
 * Switching re-applies the current knob position to the new output on the next poll.
 * VOLUME_OUT_BT requires a handler registered via volume_set_bt_handler().
 *
 * @param out  The output to drive.
 */
void volume_set_output(volume_output_t out);

/**
 * @brief The output volume_poll() currently drives (see volume_set_output()).
 * @return VOLUME_OUT_DAC (jack), VOLUME_OUT_BT (speaker) or VOLUME_OUT_NONE (mute).
 */
volume_output_t volume_get_output(void);

/**
 * @brief Override the pot with a fixed volume (for a deterministic measurement).
 *
 * While active, volume_poll() ignores the wiper and applies @p percent to the active output
 * (same curve as the equivalent knob position), so the physical knob no longer affects the
 * level. Used by the autonomy test to run at a repeatable volume. Clear with volume_clear_fixed().
 *
 * @param percent  Fixed volume, 0..100 (clamped).
 */
void volume_set_fixed(int percent);

/** @brief Release the fixed-volume override; the pot drives the level again from the next poll. */
void volume_clear_fixed(void);

/**
 * @brief The level that would be applied to the active output under the current setting.
 *
 * With a fixed override this is the exact level volume_poll() applies (a pure computation from
 * the fixed percent: the PCM5242 register byte for the DAC, the AVRCP value for Bluetooth). For
 * the autonomy CSV. Meaningful mainly while a fixed override is active.
 *
 * @return DAC volume byte (VOLUME_OUT_DAC) or AVRCP value (VOLUME_OUT_BT); 0 for VOLUME_OUT_NONE.
 */
uint8_t volume_get_level(void);

/**
 * @brief Register the Bluetooth absolute-volume setter (e.g. bluetooth_set_absolute_volume).
 *
 * Indirection so the volume service does not hard-link the Bluetooth stack when BT is unused.
 * The callback takes an AVRCP absolute volume (0..0x7F). Pass NULL to clear.
 *
 * @param set_abs_vol  The setter, or NULL.
 */
void volume_set_bt_handler(esp_err_t (*set_abs_vol)(uint8_t volume));

/**
 * @brief Trim the L/R balance to compensate the analog imbalance (driver is independent L/R).
 *
 * Applied on top of the knob level from the next poll onward. Each step is one DAC volume
 * register step (0.5 dB). Positive attenuates the RIGHT channel (the louder analog side on
 * this board), negative attenuates the LEFT. 0 = equal.
 *
 * @param steps  Signed register-step offset.
 */
void volume_set_balance(int8_t steps);

/**
 * @brief The current L/R balance trim in register steps (see volume_set_balance()).
 * @return Signed step offset; positive attenuates RIGHT, negative LEFT, 0 = equal.
 */
int8_t volume_get_balance(void);

/**
 * @brief Persist the current balance trim to NVS so it survives a reboot.
 *
 * Call sparingly (e.g. when the user leaves the audio-settings screen), not on every
 * step: the live trim is applied by volume_set_balance(), this only writes flash.
 * volume_init() restores the saved value on boot.
 */
void volume_save_balance(void);

/** @} */
