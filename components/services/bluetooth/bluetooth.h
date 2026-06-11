#pragma once

#include "esp_err.h"
#include <stdint.h>

typedef enum {
    BT_STATE_IDLE,
    BT_STATE_SCANNING,
    BT_STATE_CONNECTING,
    BT_STATE_CONNECTED,
} bt_state_t;

esp_err_t  bt_init(void);
esp_err_t  bt_scan_start(void);
esp_err_t  bt_connect(const uint8_t addr[6]);
esp_err_t  bt_disconnect(void);
bt_state_t bt_get_state(void);
