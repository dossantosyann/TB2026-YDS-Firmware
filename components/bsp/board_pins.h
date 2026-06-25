/**
 * @file board_pins.h
 * @brief GPIO pin map for the TB2026-YDS board.
 *
 * EN (FlashEN), IO0 (FlashIO0), U0RXD/U0TXD: CP2102N auto-flash + console,
 * not firmware-controlled.
 */
#pragma once

/** @name SPI2 (HSPI) — Display (NHD-1.91), native IOMUX pins */
///@{
#define PIN_DISP_MOSI    (13)
#define PIN_DISP_MISO    (-1)  ///< write-only panel
#define PIN_DISP_CLK     (14)
#define PIN_DISP_CS      (15)
#define PIN_DISP_DC      (12)  ///< MTDI strapping pin: must not be high at reset
#define PIN_DISP_RST     (2)
///@}

/** @name SPI3 (VSPI) — SD card, native IOMUX pins */
///@{
#define PIN_SD_MOSI      (23)
#define PIN_SD_MISO      (19)
#define PIN_SD_CLK       (18)
#define PIN_SD_CS        (5)
#define PIN_SD_DETECT    (21)
///@}

/** @name I2C — shared bus (expander, DAC ctrl, fuel gauge) */
///@{
#define PIN_I2C_SDA      (32)
#define PIN_I2C_SCL      (33)
///@}

/** @name I2S — PCM stream to DAC */
///@{
#define PIN_I2S_BCLK     (26)
#define PIN_I2S_WS       (25)  ///< LRCK
#define PIN_I2S_DOUT     (27)
///@}

/** @name ADC — potentiometer (SENSOR_VP) */
///@{
#define PIN_POT_ADC      (36)
#define POT_ADC_CHANNEL  ADC_CHANNEL_0  ///< ADC1
///@}

/** @name Inputs — GPIO34..39 are input-only, no internal pulls (external pulls required) */
///@{
#define PIN_BTN_A        (39)  ///< SENSOR_VN
#define PIN_BATT_ALERT   (34)  ///< MAX17260 ALRT
#define PIN_EXPANDER_INT (35)  ///< PI4IOE5V9554A INT
///@}

/** @name Control outputs */
///@{
#define PIN_REG_EN       (4)   ///< EnableReg
#define PIN_USB_DIR      (22)  ///< TC7USB40MU USB mux select
///@}

/** @name GPIO expander channels (PI4IOE5V9554A on I2C 0x38) — IO0..IO7 bit positions.
 *  These are channel indices on the expander, not ESP32 GPIOs. Pin polarity (XSMT/SHDN
 *  active-low, etc.) lives in the drivers, not here. */
///@{
#define EXP_DAC_MUTE     (0)   ///< IO0: PCM5242 XSMT (DAC soft mute)
#define EXP_AMP_SHDN     (1)   ///< IO1: MAX97220 shutdown
#define EXP_BTN_B        (2)   ///< IO2: button B
#define EXP_BTN_RIGHT    (3)   ///< IO3: button Right
#define EXP_BTN_LEFT     (4)   ///< IO4: button Left
#define EXP_BTN_DOWN     (5)   ///< IO5: button Down
#define EXP_BTN_UP       (6)   ///< IO6: button Up
#define EXP_INOKB        (7)   ///< IO7: MAX17260 fuel-gauge INOKB (input)
///@}
