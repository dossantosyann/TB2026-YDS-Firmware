/**
 * @file maintenance.h
 * @brief Maintenance service: low-priority background task for periodic chores.
 *
 * Owns a single shared housekeeping task that runs slow periodic work off the
 * critical paths. Tenants: volume and player polling, the power service
 * (power_tick + idle auto power-off), the autonomy test, the SD hot-plug watch
 * and the diag dump.
 */
#pragma once

#include "esp_err.h"

/**
 * @defgroup services_maintenance Maintenance service
 * @ingroup services
 * @brief Shared low-priority task that periodically drives background chores.
 * @{
 */

/**
 * @brief Start the maintenance task.
 *
 * Idempotent. The drivers used by the tenants (fuel gauge, GPIO expander) must be
 * initialised before this runs.
 *
 * @return ESP_OK on success, ESP_ERR_NO_MEM if the task cannot be created.
 */
esp_err_t maintenance_init(void);

/** @} */
