#include "display_oled.h"

/* Deferred until the board is in hand: the SSD1333 init sequence and the SPI DMA blit
   can only be verified against the panel. These stubs keep the gfx->display layer
   linked so the framebuffer renderer builds and is host-testable without hardware. */

esp_err_t display_oled_init(spi_device_handle_t spi)
{
    (void)spi;
    return ESP_OK;
}

esp_err_t display_oled_draw(const uint8_t *buf, size_t len)
{
    (void)buf;
    (void)len;
    return ESP_OK;
}

esp_err_t display_oled_clear(void)
{
    return ESP_OK;
}
