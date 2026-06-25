#include "app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"      /* TEST CODE */
#include "esp_err.h"      /* TEST CODE */
#include "gfx.h"          /* TEST CODE */
#include "spi_bus.h"      /* TEST CODE */
#include "display_oled.h" /* TEST CODE */
#include "driver/gpio.h"
#include "board_pins.h"

void app_init(void)
{
    /* Self-hold power: drive EnableReg (GPIO4) HIGH before anything else.
       The regulator is only latched on by USB/the power button at boot; if the
       ESP32 doesn't take over the hold immediately, releasing them cuts power.
       Preload the output register high, THEN enable the driver, so the pin never
       drives low (even briefly) on its way up. */
    gpio_set_level(PIN_REG_EN, 1);
    gpio_set_direction(PIN_REG_EN, GPIO_MODE_OUTPUT);

    /* ===== TEST CODE — remove once the display is wired in for real ===== */
    spi_device_handle_t disp_spi;
    ESP_ERROR_CHECK(spi_bus_display_init(&disp_spi));
    ESP_ERROR_CHECK(display_oled_init(disp_spi));
    gfx_clear(GFX_BLACK);
    gfx_draw_text(43, 84, "ca fonctionne !", GFX_WHITE, 1);
    gfx_flush();
    ESP_LOGI("test", "display init + gfx flushed");

    /* ===== TEST CODE — button A health check (GPIO39, input-only, no internal pull).
       WARNING: this button drives ~4.2V (VBAT) when pressed, above the GPIO abs-max
       (~3.6V). Reading is passive; keep presses short until the hardware is fixed.
       Poll loop lives in app_run(). ===== */
    const gpio_config_t btn_a = {
        .pin_bit_mask = 1ULL << PIN_BTN_A,
        .mode = GPIO_MODE_INPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&btn_a));
    /* ===== END TEST CODE ===== */

    // TODO: volume pot (adc.h) — adc_pot_init() once at startup, then read in the
    // maintenance/UI task, not here. No ULP: it only saves power while the main cores
    // deep-sleep, which can't happen during playback, and the read is negligible next
    // to the OLED/DAC/amp/radio. Power levers instead:
    //   - poll at ~10-20 Hz and only I2C-write the PCM5242 when the volume byte changes
    //   - POT_SAMPLES 16 is generous for a human knob; 8 (or 4 + deadband) is plenty
    //   - deep-sleep when idle (screen off, no audio), waking on a button GPIO

    // TODO: audio out (i2s_bus.h) — i2s_bus_init() once at startup; the decoder
    // calls i2s_bus_reconfig(tx, rate, bits) per track. 3-wire I2S to the PCM5242
    // (I2C 0x4C): BCK=26, WS/LRCK=25, DOUT=27, no MCLK — 32-bit slots give BCK=64*fS,
    // which the DAC's PLL locks onto to make its own clock. Format/PLL/volume live in
    // the DAC's registers over I2C (vol: L=Pg0 R0x3D, R=0x3E; 0x00=+24dB .. 0xFE=-103dB,
    // 0xFF=mute), so adc_pot_to_volume() bytes get written there, not to I2S.
    //
    // Verify on the schematic before relying on the above (see memory pcm5242-audio-path):
    //   - PCM5242 DVDD = 3.3V (ESP32 logic levels + 24.576 MHz BCK ceiling)
    //   - SCK (pin 26) tied to GND (needed to start the 3-wire BCK-PLL)
    //   - MODE1/MODE2 strapped for I2C (0x4C)
}

void app_run(void)
{
    /* ===== TEST CODE — log button A level only when it changes ===== */
    int last = -1;
    for (;;) {
        int lvl = gpio_get_level(PIN_BTN_A);
        if (lvl != last) {
            ESP_LOGI("btn", "button A = %d (%s)", lvl, lvl ? "PRESSED (high)" : "released (low)");
            last = lvl;
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    /* ===== END TEST CODE ===== */
}
