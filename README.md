# TB2026-YDS — ESP32 MP3 Player Firmware

Firmware for a portable MP3 player built around an **ESP32-WROVER-E** (with PSRAM),
developed as a Bachelor thesis project (TB 2026). Written in C on ESP-IDF / FreeRTOS.

## Hardware overview

| Domain   | Parts |
|----------|-------|
| **MCU**  | ESP32-WROVER-E (PSRAM), flashed via CP2102N (auto-flash). USB-C routed by a PI3USB221AZUAEX mux to either CP2102N or charger |
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

## Repository layout

```
components/bsp        SPI / I2C / I2S buses + volume ADC
components/drivers    chip drivers (DAC, charger, fuel gauge, OLED, GPIO expander…)
components/services   audio, bluetooth, storage, power, settings, autonomy, diag…
components/screens    per-screen UI (framebuffer, no LVGL)
components/ui         rendering + navigation core
main                  app entry point and task setup
test/host            off-target unit tests (run on the dev machine)
```

## Build & flash

Requires **ESP-IDF 6.0.1** and the custom **TB2026-YDS** board (flash and serial
console over USB-C, CP2102N auto-flash).

```bash
idf.py build        # compile
idf.py flash        # flash over USB-C (CP2102N auto-flash)
idf.py monitor      # serial console
```

## Documentation

API reference is generated with **Doxygen** and published online:

**<https://tb2026-firmware.yanndossantos.ch>**

To rebuild it locally:

```bash
doxygen Doxyfile        # output in html/index.html
```

Browse the **Topics** tab for the logical module tree (e.g. *Board Support Package*),
or the **Files** tab for the per-file view and the GPIO pin map.

## Status

Working on target: audio playback (jack + Bluetooth A2DP), microSD library and
navigation, OLED UI, charging and fuel-gauge readout, power-saving and auto-off.

Known limitations: headerless VBR MP3 duration is reported as unknown; the
self-measured battery-autonomy test is built but its full-discharge validation is
still pending; some Bluetooth reliability edge cases are under active work.

## AI assistance

Most of this firmware's source was developed with the assistance of **Claude Opus 4.8**
(Anthropic), under the author's direction: specification, review, and on-target validation
were carried out by Y. Dos Santos. Every source file under `components/` and `main/` carries
a Doxygen `@note` recording this, and each AI-assisted commit is co-signed
`Co-Authored-By: Claude Opus`, so the full contribution history is traceable via `git log`.
