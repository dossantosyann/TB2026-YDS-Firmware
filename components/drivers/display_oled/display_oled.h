#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"

esp_err_t display_oled_init(spi_device_handle_t spi);
esp_err_t display_oled_draw(const uint8_t *buf, size_t len);
esp_err_t display_oled_clear(void);
