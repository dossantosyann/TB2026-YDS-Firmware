#pragma once

#include "esp_err.h"

/* MAX97220 — enable/mute via GPIO expander */
esp_err_t headphone_amp_enable(void);
esp_err_t headphone_amp_mute(void);
