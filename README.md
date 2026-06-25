# TB2026-YDS — ESP32 MP3 Player Firmware

Firmware for a portable MP3 player built around an **ESP32-WROVER-E** (with PSRAM),
developed as a Bachelor thesis project (TB 2026). Written in C on ESP-IDF / FreeRTOS.

## Hardware overview

| Domain   | Parts |
|----------|-------|
| **MCU**  | ESP32-WROVER-E (PSRAM), flashed via CP2102N (auto-flash). USB-C routed by a TC7USB40MU mux to either CP2102N or charger |
| **Audio**| Differential chain PCM5242 (I²S) → RC filter → MAX97220 → 3.5 mm jack. Bluetooth A2DP output (AVRCP volume) |

> **Sample rates:** the PCM5242 derives its PLL from BCK (= 64·fS, SCK grounded). The PLL
> needs BCK ≥ 1 MHz, so rates **below 16 kHz cannot lock and are rejected** by
> `audio_dac_set_sample_rate()`. Supported: 16, 22.05, 24, 32, 44.1, 48, 88.2, 96, 176.4, 192 kHz.
| **Power**| MAX77757 charger, MAX17260 fuel gauge (I²C), TPS62A01 buck, XC6120 supervisor (deep-discharge protection) |
| **UI**   | 176×176 SPI OLED (SSD1333), SPI microSD (FATFS), I²C GPIO expander PI4IOE5V9554A (buttons, DAC mute, amp shutdown), volume potentiometer on ADC |

## Architecture

Layered C, from the hardware up:

```
bsp        SPI / I2C / I2S buses + volume ADC
drivers    chip drivers on top of the buses
services   audio pipeline, bluetooth, storage…
ui/screens framebuffer rendering (no LVGL) + navigation
```

A high-priority pinned audio task, a UI task and a maintenance task run under
FreeRTOS; inputs arrive via interrupt into a single event queue.

## Build & flash

Requires **ESP-IDF 6.0.1**.

```bash
idf.py build        # compile
idf.py flash        # flash over USB-C (CP2102N auto-flash)
idf.py monitor      # serial console
```

## Documentation

API reference is generated with **Doxygen**:

```bash
doxygen Doxyfile        # output in html/index.html
```

Browse the **Topics** tab for the logical module tree (e.g. *Board Support Package*),
or the **Files** tab for the per-file view and the GPIO pin map.
