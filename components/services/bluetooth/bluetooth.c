#include "bluetooth.h"

esp_err_t  bt_init(void)                        { return ESP_OK; }
esp_err_t  bt_scan_start(void)                  { return ESP_OK; }
esp_err_t  bt_connect(const uint8_t addr[6])    { return ESP_OK; }
esp_err_t  bt_disconnect(void)                  { return ESP_OK; }
bt_state_t bt_get_state(void)                   { return BT_STATE_IDLE; }
