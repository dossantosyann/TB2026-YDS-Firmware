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

#include <stdbool.h>
#include <stdint.h>
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

/**
 * @brief Register-level snapshot for the debug page: config vs expected + learned state.
 *
 * The *_want fields are the raw values fuel_gauge_init() writes during EZ config, so the
 * UI can flag a gauge that lost its configuration (POR) or holds different constants.
 * Raw capacity LSB is 1 mAh with the board's 5 mOhm sense resistor.
 */
typedef struct {
    bool     por;               ///< Status.POR: the gauge reset and lost its configuration.
    uint16_t design_cap_raw;    ///< DesignCap (0x18) as read back.
    uint16_t design_cap_want;   ///< DesignCap value EZ config writes.
    uint16_t ichg_term_raw;     ///< IChgTerm (0x1E) as read back.
    uint16_t ichg_term_want;    ///< IChgTerm value EZ config writes.
    uint16_t vempty_raw;        ///< VEmpty (0x3A) as read back.
    uint16_t vempty_want;       ///< VEmpty value EZ config writes.
    float    full_cap_rep_mah;  ///< FullCapRep: learned reported full capacity, mAh.
    float    full_cap_nom_mah;  ///< FullCapNom: learned nominal full capacity, mAh.
} fuel_gauge_debug_t;

/**
 * @brief Read the configuration and learned-capacity registers for the debug page.
 *
 * @param[out] out  Receives the snapshot.
 * @return ESP_OK on success, or the first I2C error encountered.
 */
esp_err_t fuel_gauge_read_debug(fuel_gauge_debug_t *out);

/**
 * @brief Re-run the EZ model load to purge the learned battery state.
 *
 * Preserves the cycle counter across the reload; everything else learned (full capacity,
 * hence health) restarts from the design values, and SOC is re-estimated from voltage —
 * best done at rest, right after a charge the estimate overshoots for a few minutes.
 *
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if the model refresh never completes,
 *         ESP_ERR_INVALID_RESPONSE if a config write does not verify, or the I2C error.
 */
esp_err_t fuel_gauge_reload_ez(void);

/** @} */
