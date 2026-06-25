/**
 * @file fuel_gauge.h
 * @brief MAX17260 ModelGauge m5 EZ 1-cell fuel gauge (I2C 0x36).
 *
 * One-shot EZ configuration on power-up (POR), then a snapshot read of the
 * application-facing measurements. Current/capacity scaling assumes the board's
 * 5 mOhm sense resistor; the cell parameters (design capacity, charge-termination
 * current) are compile-time constants in fuel_gauge.c — change them with the cell.
 *
 * No NTC is fitted, so the temperature field is the IC's internal die temperature,
 * not the battery temperature.
 */
#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

/**
 * @defgroup drivers_fuel_gauge Fuel gauge (MAX17260)
 * @ingroup drivers
 * @brief I2C access to the ModelGauge m5 EZ state-of-charge and analog measurements.
 * @{
 */

/**
 * @brief Snapshot of the gauge's application-facing outputs.
 *
 * Filled in one call by fuel_gauge_read(). All values are already converted to
 * physical units.
 */
typedef struct {
    float soc_pct;      ///< RepSOC: reported state of charge, %.
    float voltage_v;    ///< VCell: cell voltage, V.
    float current_ma;   ///< Current: + into the cell (charge), - out (discharge), mA.
    float rep_cap_mah;  ///< RepCap: reported remaining capacity, mAh.
    float tte_s;        ///< TTE: time to empty, s (only valid while discharging).
    float ttf_s;        ///< TTF: time to full, s (only valid while charging).
    float age_pct;      ///< Age: present full capacity vs. design capacity, % (cell health).
    float die_temp_c;   ///< Temp: IC die temperature (no NTC fitted), degC.
    float cycles;       ///< Cycles: accumulated full-charge/discharge equivalents.
} fuel_gauge_data_t;

/**
 * @brief Attach the gauge to the shared I2C bus and run EZ config if needed.
 *
 * Reads the POR flag: if the gauge has already been configured (POR clear), the
 * learned battery state is kept and only the device handle is registered. On a
 * fresh power-up it loads the EZ model (design capacity, charge-termination
 * current, empty voltage) and clears POR.
 *
 * @param bus  Master bus handle from i2c_bus_init().
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if the gauge never reports data
 *         ready / model refresh, ESP_ERR_INVALID_RESPONSE if a config write does
 *         not verify, or the underlying I2C error.
 */
esp_err_t fuel_gauge_init(i2c_master_bus_handle_t bus);

/**
 * @brief Read and convert all fields of a fuel_gauge_data_t in one pass.
 *
 * @param[out] out  Receives the converted measurements.
 * @return ESP_OK on success, or the first I2C error encountered.
 */
esp_err_t fuel_gauge_read(fuel_gauge_data_t *out);

/** @} */
