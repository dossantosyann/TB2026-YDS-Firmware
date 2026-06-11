#include "app.h"
#include "bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app";

static i2c_master_bus_handle_t s_i2c_bus;
static spi_device_handle_t s_display_spi;

void app_init(void)
{
    ESP_ERROR_CHECK(i2c_bus_init(&s_i2c_bus));
    ESP_LOGI(TAG, "I2C bus ready, scanning...");
    i2c_bus_scan(s_i2c_bus);

    ESP_ERROR_CHECK(spi_bus_display_init(&s_display_spi));
    ESP_ERROR_CHECK(spi_bus_sdcard_init());
    ESP_LOGI(TAG, "SPI buses ready (display + sdcard)");
}

void app_run(void)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
