/* Minimal host fake of esp_gap_bt_api.h: just the types/macros bluetooth.h needs in its
   prototypes. The host tests never exercise the GAP/scan path, so these are enough to compile. */
#pragma once

#include <stdint.h>

#define ESP_BD_ADDR_LEN            6
#define ESP_BT_GAP_MAX_BDNAME_LEN  248

typedef uint8_t esp_bd_addr_t[ESP_BD_ADDR_LEN];
