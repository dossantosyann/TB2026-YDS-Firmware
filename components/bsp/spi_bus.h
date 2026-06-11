#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"

/* Display on SPI2 (HSPI), SD card on SPI3 (VSPI) — both on native IOMUX pins. */
#define SPI_BUS_DISPLAY_HOST SPI2_HOST
#define SPI_BUS_SDCARD_HOST  SPI3_HOST

/* Init SPI2 and add the display as a device; returns its handle. */
esp_err_t spi_bus_display_init(spi_device_handle_t *out_handle);

/* Init SPI3 only; the sdcard driver attaches itself via sdspi
 * (host = SPI_BUS_SDCARD_HOST, CS = PIN_SD_CS). */
esp_err_t spi_bus_sdcard_init(void);
