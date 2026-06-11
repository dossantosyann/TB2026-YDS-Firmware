#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"

esp_err_t spi_bus_display_init(spi_device_handle_t *out_handle);
esp_err_t spi_bus_sdcard_init(spi_device_handle_t *out_handle);
