/**
 * @file settings.h
 * @brief Settings service: persistent user-preference store backed by NVS.
 *
 * A thin, generic key/value wrapper over a single NVS namespace ("settings").
 * Other services and screens use it to remember a value across reboots without
 * each one re-implementing its own NVS open/commit dance. The service does not
 * know what any key means: callers own the keys, the types, and the defaults.
 *
 * Typical use (e.g. OLED brightness, last-played position):
 * @code
 *   uint8_t brightness;
 *   if (settings_get_u8("oled_bri", &brightness) != ESP_OK)
 *       brightness = OLED_BRIGHTNESS_DEFAULT;   // first boot: nothing stored yet
 *   ...
 *   settings_set_u8("oled_bri", brightness);    // persist a change
 * @endcode
 *
 * This service owns no task. Each accessor opens, reads/writes, commits, and
 * closes the namespace; saves are rare (a user changing a preference), so the
 * per-call handle keeps the implementation simple and stateless.
 *
 * Bluetooth keeps its own NVS namespace ("bt") for the known-device list; this
 * store is for scalar preferences, not that blob.
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @defgroup services_settings Settings service
 * @ingroup services
 * @brief Persistent key/value store for user preferences (NVS-backed).
 * @{
 */

/**
 * @brief Initialise NVS so the settings accessors can be used.
 *
 * Safe to call more than once and from any service's init: the underlying
 * nvs_flash_init() is idempotent. Recovers a corrupt/outdated NVS partition by
 * erasing and re-initialising it.
 *
 * @return ESP_OK on success, or an esp_err_t from nvs_flash_init().
 */
esp_err_t settings_init(void);

/**
 * @brief Read a stored uint8 preference.
 * @param[in]  key  NVS key (<= 15 chars).
 * @param[out] out  Receives the stored value; untouched on error.
 * @return ESP_OK, ESP_ERR_NVS_NOT_FOUND if the key was never set (use your
 *         default), or another esp_err_t.
 */
esp_err_t settings_get_u8(const char *key, uint8_t *out);

/** @brief Store a uint8 preference and commit it. @see settings_get_u8 */
esp_err_t settings_set_u8(const char *key, uint8_t val);

/**
 * @brief Read a stored uint32 preference.
 * @param[in]  key  NVS key (<= 15 chars).
 * @param[out] out  Receives the stored value; untouched on error.
 * @return ESP_OK, ESP_ERR_NVS_NOT_FOUND if absent, or another esp_err_t.
 */
esp_err_t settings_get_u32(const char *key, uint32_t *out);

/** @brief Store a uint32 preference and commit it. @see settings_get_u32 */
esp_err_t settings_set_u32(const char *key, uint32_t val);

/**
 * @brief Read a stored NUL-terminated string preference.
 * @param[in]  key       NVS key (<= 15 chars).
 * @param[out] out       Buffer to receive the string (always NUL-terminated on
 *                       ESP_OK).
 * @param[in]  out_size  Size of @p out in bytes.
 * @return ESP_OK, ESP_ERR_NVS_NOT_FOUND if absent, ESP_ERR_NVS_INVALID_LENGTH
 *         if @p out is too small, or another esp_err_t.
 */
esp_err_t settings_get_str(const char *key, char *out, size_t out_size);

/** @brief Store a NUL-terminated string preference and commit it. @see settings_get_str */
esp_err_t settings_set_str(const char *key, const char *val);

/** @} */
