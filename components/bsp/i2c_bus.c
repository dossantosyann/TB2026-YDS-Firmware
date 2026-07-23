/**
 * @file i2c_bus.c
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "i2c_bus.h"
#include "board_pins.h"
#include "esp_log.h"

static const char *TAG = "i2c_bus";

/* Cached so diagnostics (and any later consumer) can reach the bus without app.c
   having to hand its handle around. Set once by i2c_bus_init(). */
static i2c_master_bus_handle_t s_bus;

esp_err_t i2c_bus_init(i2c_master_bus_handle_t *out_handle)
{
    const i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,              // GPIO32
        .scl_io_num = PIN_I2C_SCL,              // GPIO33
        .clk_source = I2C_CLK_SRC_DEFAULT,      // APB clock feeds the peripheral
        .glitch_ignore_cnt = 7,                 // hardware noise filter
        .flags.enable_internal_pullup = false,  // PCB has external pull-ups
    };
    esp_err_t err = i2c_new_master_bus(&cfg, out_handle);
    if (err == ESP_OK) {
        s_bus = *out_handle;
    }
    return err;
}

i2c_master_bus_handle_t i2c_bus_handle(void)
{
    return s_bus;
}

void i2c_bus_scan(i2c_master_bus_handle_t bus)
{
    int found = 0;
    for (uint16_t addr = 0x08; addr <= 0x77; addr++) {
        if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
            ESP_LOGI(TAG, "device at 0x%02X", addr);
            found++;
        }
    }
    ESP_LOGI(TAG, "scan done, %d device(s) found", found);
}
