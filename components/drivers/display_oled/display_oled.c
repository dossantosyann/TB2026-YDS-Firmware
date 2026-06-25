#include "display_oled.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

/* NHD-1.91-176176UBC3 panel, SSD1333 controller, 4-wire SPI.
   Init sequence and gamma LUT taken verbatim from the NHD datasheet example
   (Init_OLED, p.17-18); the Set_* helpers there map to the Solomon command bytes
   decoded below. */

#define DISP_W 176
#define DISP_H 176

/* SSD1333 commands used here */
#define CMD_COMMAND_LOCK   0xFD
#define CMD_DISPLAY_OFF    0xAE
#define CMD_DISPLAY_ON     0xAF
#define CMD_REMAP_FORMAT   0xA0
#define CMD_START_LINE     0xA1
#define CMD_DISPLAY_OFFSET 0xA2
#define CMD_DISPLAY_NORMAL 0xA6
#define CMD_PHASE_LENGTH   0xB1
#define CMD_DISPLAY_CLOCK  0xB3
#define CMD_PRECHARGE_PER  0xB6
#define CMD_GAMMA          0xB8
#define CMD_PRECHARGE_VOLT 0xBB
#define CMD_VCOMH          0xBE
#define CMD_CONTRAST_COLOR 0xC1
#define CMD_MASTER_CURRENT 0xC7
#define CMD_MUX_RATIO      0xCA
#define CMD_SET_COLUMN     0x15
#define CMD_SET_ROW        0x75
#define CMD_WRITE_RAM      0x5C

static spi_device_handle_t s_spi;
static uint8_t s_line[DISP_W * 2];   /* one RGB565 row, byte-swapped, DMA-capable */

/* Datasheet gamma look-up table (63 entries, cmd 0xB8). */
static const uint8_t s_gamma[63] = {
    0x00,0x02,0x04,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x10,0x11,0x12,0x14,
    0x15,0x16,0x18,0x1a,0x1b,0x1d,0x1f,0x20,0x22,0x24,0x26,0x27,0x29,0x2b,0x2d,0x2f,
    0x31,0x33,0x35,0x37,0x39,0x3b,0x3d,0x3f,0x42,0x44,0x46,0x48,0x4b,0x4d,0x50,0x52,
    0x55,0x57,0x5a,0x5b,0x5f,0x62,0x65,0x67,0x6a,0x6d,0x70,0x73,0x76,0x79,0x7c,
};

static esp_err_t spi_tx(const uint8_t *data, size_t len)
{
    spi_transaction_t t = { .length = len * 8 };
    if (len <= 4) {                          /* small: avoid DMA, use inline buffer */
        t.flags = SPI_TRANS_USE_TXDATA;
        memcpy(t.tx_data, data, len);
    } else {
        t.tx_buffer = data;                  /* caller guarantees DMA-capable memory */
    }
    return spi_device_polling_transmit(s_spi, &t);
}

static void write_cmd(uint8_t c)
{
    gpio_set_level(PIN_DISP_DC, 0);          /* D/C# low = command */
    spi_tx(&c, 1);
}

static void write_data(const uint8_t *d, size_t n)
{
    gpio_set_level(PIN_DISP_DC, 1);          /* D/C# high = data */
    spi_tx(d, n);
}

static void write_data1(uint8_t d)
{
    write_data(&d, 1);
}

/* Set the RAM address window to the full panel and arm a RAM write. */
static void set_full_window(void)
{
    uint8_t col[2] = { 0, DISP_W - 1 };
    uint8_t row[2] = { 0, DISP_H - 1 };
    write_cmd(CMD_SET_COLUMN); write_data(col, 2);
    write_cmd(CMD_SET_ROW);    write_data(row, 2);
    write_cmd(CMD_WRITE_RAM);
}

esp_err_t display_oled_init(spi_device_handle_t spi)
{
    s_spi = spi;

    const gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_DISP_DC) | (1ULL << PIN_DISP_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) return err;
    gpio_set_level(PIN_DISP_DC, 0);          /* PIN_DISP_DC=GPIO12 is a strapping pin: keep low */

    /* Hardware reset pulse (datasheet: low 10 ms, then high). */
    gpio_set_level(PIN_DISP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_DISP_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    write_cmd(CMD_COMMAND_LOCK);   write_data1(0x12);   /* unlock driver IC */
    write_cmd(CMD_DISPLAY_OFF);
    write_cmd(CMD_REMAP_FORMAT);   write_data1(0x74);   /* horizontal increment, 65k color,
                                                            bit4=COM scan reversed: vertical flip
                                                            to match the panel mounting (datasheet
                                                            example used 0x64) */
    write_cmd(CMD_START_LINE);     write_data1(0x00);
    write_cmd(CMD_DISPLAY_OFFSET); write_data1(0x00);
    write_cmd(CMD_DISPLAY_NORMAL);
    write_cmd(CMD_PHASE_LENGTH);   write_data1(0x24);
    write_cmd(CMD_DISPLAY_CLOCK);  write_data1(0x80);
    write_cmd(CMD_PRECHARGE_PER);  write_data1(0x0F);
    write_cmd(CMD_PRECHARGE_VOLT); write_data1(0x1F);   /* 0.40 * VCC */
    write_cmd(CMD_VCOMH);          write_data1(0x05);   /* 0.82 * VCC */
    { uint8_t c[3] = { 0x3C, 0x39, 0x43 };
      write_cmd(CMD_CONTRAST_COLOR); write_data(c, 3); }
    write_cmd(CMD_MASTER_CURRENT); write_data1(0x0F);
    write_cmd(CMD_MUX_RATIO);      write_data1(0xAF);   /* 176 rows */
    write_cmd(CMD_GAMMA);          write_data(s_gamma, sizeof s_gamma);
    write_cmd(CMD_DISPLAY_ON);
    vTaskDelay(pdMS_TO_TICKS(100));          /* let the panel bias settle before drawing */

    return ESP_OK;
}

esp_err_t display_oled_draw(const uint8_t *buf, size_t len)
{
    if (len != DISP_W * DISP_H * 2) return ESP_ERR_INVALID_SIZE;

    const uint16_t *px = (const uint16_t *)buf;
    set_full_window();
    gpio_set_level(PIN_DISP_DC, 1);          /* data for the whole RAM stream */
    for (int y = 0; y < DISP_H; y++) {
        for (int x = 0; x < DISP_W; x++) {
            uint16_t p = px[y * DISP_W + x]; /* native RGB565 -> panel wants MSB first */
            s_line[x * 2]     = (uint8_t)(p >> 8);
            s_line[x * 2 + 1] = (uint8_t)(p & 0xFF);
        }
        esp_err_t err = spi_tx(s_line, sizeof s_line);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

esp_err_t display_oled_clear(void)
{
    memset(s_line, 0, sizeof s_line);
    set_full_window();
    gpio_set_level(PIN_DISP_DC, 1);
    for (int y = 0; y < DISP_H; y++) {
        esp_err_t err = spi_tx(s_line, sizeof s_line);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}
