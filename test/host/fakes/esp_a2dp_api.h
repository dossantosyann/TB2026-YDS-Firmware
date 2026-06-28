/* Minimal host fake of esp_a2dp_api.h: just the one type bluetooth.h needs in its prototypes.
   The host tests never call the A2DP data path, so a matching typedef is enough to compile. */
#pragma once

#include <stdint.h>

typedef int32_t (*esp_a2d_source_data_cb_t)(uint8_t *buf, int32_t len);
