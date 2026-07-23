/**
 * @file i2c_bus.h
 * @brief Shared I2C master bus (expander, DAC control, fuel gauge).
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

/**
 * @defgroup bsp_i2c I2C bus
 * @ingroup bsp
 * @brief Shared I2C master bus (expander, DAC control, fuel gauge).
 * @{
 */

/** Per-device SCL speed, set when drivers add themselves to the bus. */
#define I2C_BUS_FREQ_HZ (400 * 1000)

/**
 * @brief Initialize the shared I2C master bus on the board's SDA/SCL pins.
 *
 * @param[out] out_handle  Receives the master bus handle.
 * @return ESP_OK on success, or the underlying driver error.
 */
esp_err_t i2c_bus_init(i2c_master_bus_handle_t *out_handle);

/**
 * @brief The shared bus handle cached by i2c_bus_init().
 *
 * Lets a consumer that didn't run init (e.g. the stats diagnostics page probing
 * device presence) reach the bus without threading the handle through every layer.
 *
 * @return The master bus handle, or NULL if i2c_bus_init() has not succeeded yet.
 */
i2c_master_bus_handle_t i2c_bus_handle(void);

/**
 * @brief Probe 0x08..0x77 and log every ACKing address. Bring-up helper.
 *
 * @param bus  Master bus handle from i2c_bus_init().
 */
void i2c_bus_scan(i2c_master_bus_handle_t bus);

/** @} */
