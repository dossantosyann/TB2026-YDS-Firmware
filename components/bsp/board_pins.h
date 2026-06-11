#pragma once

/* EN (FlashEN), IO0 (FlashIO0), U0RXD/U0TXD: CP2102N auto-flash + console, not firmware-controlled. */

/* SPI2 (HSPI) — Display (NHD-1.91), native IOMUX pins */
#define PIN_DISP_MOSI    (13)
#define PIN_DISP_MISO    (-1)  /* write-only panel */
#define PIN_DISP_CLK     (14)
#define PIN_DISP_CS      (15)
#define PIN_DISP_DC      (12)  /* MTDI strapping pin: must not be high at reset */
#define PIN_DISP_RST     (2)

/* SPI3 (VSPI) — SD card, native IOMUX pins */
#define PIN_SD_MOSI      (23)
#define PIN_SD_MISO      (19)
#define PIN_SD_CLK       (18)
#define PIN_SD_CS        (5)
#define PIN_SD_DETECT    (21)

/* I2C — shared bus (expander, DAC ctrl, fuel gauge) */
#define PIN_I2C_SDA      (32)
#define PIN_I2C_SCL      (33)

/* I2S — PCM stream to DAC */
#define PIN_I2S_BCLK     (26)
#define PIN_I2S_WS       (25)  /* LRCK */
#define PIN_I2S_DOUT     (27)

/* ADC — potentiometer (SENSOR_VP) */
#define PIN_POT_ADC      (36)
#define POT_ADC_CHANNEL  ADC_CHANNEL_0  /* ADC1 */

/* Inputs — GPIO34..39 are input-only, no internal pulls (external pulls required) */
#define PIN_BTN_A        (39)  /* SENSOR_VN */
#define PIN_BATT_ALERT   (34)  /* MAX17260 ALRT */
#define PIN_EXPANDER_INT (35)  /* PI4IOE5V9554A INT */

/* Control outputs */
#define PIN_REG_EN       (4)   /* EnableReg */
#define PIN_USB_DIR      (22)  /* TC7USB40MU USB mux select */
