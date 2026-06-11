#include "spi_bus.h"
#include "board_pins.h"

#define DISPLAY_SPI_HZ      (10 * 1000 * 1000) /* confirmed by SSD1333 datasheet */
#define DISPLAY_FRAME_BYTES (176 * 176 * 2)    /* full RGB565 frame, sets DMA transfer cap */

esp_err_t spi_bus_display_init(spi_device_handle_t *out_handle)
{
    const spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_DISP_MOSI,
        .miso_io_num = PIN_DISP_MISO, /* -1: write-only panel */
        .sclk_io_num = PIN_DISP_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_FRAME_BYTES,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_IOMUX_PINS,
    };
    esp_err_t err = spi_bus_initialize(SPI_BUS_DISPLAY_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        return err;
    }

    const spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = DISPLAY_SPI_HZ,
        .mode = 0, /* clock idle low, latch on rising edge (datasheet 4-wire SPI timing) */
        .spics_io_num = PIN_DISP_CS,
        .queue_size = 2,
    };
    return spi_bus_add_device(SPI_BUS_DISPLAY_HOST, &dev_cfg, out_handle);
}

esp_err_t spi_bus_sdcard_init(void)
{
    const spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_SD_MOSI,
        .miso_io_num = PIN_SD_MISO,
        .sclk_io_num = PIN_SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_IOMUX_PINS,
    };
    return spi_bus_initialize(SPI_BUS_SDCARD_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
}
