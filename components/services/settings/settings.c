/**
 * @file settings.c
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "settings.h"

#include "nvs_flash.h"
#include "nvs.h"

#define SETTINGS_NVS_NS  "settings"   /* NVS namespace owned by this service */

esp_err_t settings_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t settings_get_u8(const char *key, uint8_t *out)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(SETTINGS_NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    err = nvs_get_u8(h, key, out);
    nvs_close(h);
    return err;
}

esp_err_t settings_set_u8(const char *key, uint8_t val)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(SETTINGS_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, key, val);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t settings_get_u32(const char *key, uint32_t *out)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(SETTINGS_NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    err = nvs_get_u32(h, key, out);
    nvs_close(h);
    return err;
}

esp_err_t settings_set_u32(const char *key, uint32_t val)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(SETTINGS_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u32(h, key, val);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t settings_get_str(const char *key, char *out, size_t out_size)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(SETTINGS_NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    err = nvs_get_str(h, key, out, &out_size);
    nvs_close(h);
    return err;
}

esp_err_t settings_set_str(const char *key, const char *val)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(SETTINGS_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, key, val);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
