#include "app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"            /* TEST CODE */
#include "esp_err.h"            /* TEST CODE */
#include "gfx.h"               /* TEST CODE */
#include "spi_bus.h"           /* TEST CODE */
#include "i2c_bus.h"           /* TEST CODE */
#include "i2s_bus.h"           /* TEST CODE */
#include "adc.h"               /* TEST CODE */
#include "display_oled.h"      /* TEST CODE */
#include "driver/i2c_master.h" /* TEST CODE */
#include <stdio.h>             /* TEST CODE */
#include <string.h>            /* TEST CODE */
#include "driver/gpio.h"
#include "board_pins.h"

/* ===== TEST CODE — bsp bring-up. Probes each bus and shows pass/fail on the OLED.
   Remove this whole block once the real drivers/services own the buses. ===== */
#define COL_OK   gfx_rgb(0, 220, 0)
#define COL_FAIL gfx_rgb(220, 0, 0)

/* Known I2C devices on the shared bus (see memory hardware-bringup). */
#define ADDR_GAUGE 0x36  /* MAX17260 fuel gauge */
#define ADDR_EXP   0x38  /* PI4IOE5V9554A expander */
#define ADDR_DAC   0x4C  /* PCM5242 DAC */

static i2c_master_bus_handle_t   s_i2c;
static adc_oneshot_unit_handle_t s_adc;
static i2s_chan_handle_t         s_i2s;

static bool s_gauge_ok, s_exp_ok, s_dac_ok, s_i2s_ok, s_adc_ok;

/* Draw "label" in white, then OK (green) / FAIL (red) right after it. */
static void draw_status(int x, int y, const char *label, bool ok)
{
    gfx_draw_text(x, y, label, GFX_WHITE, 1);
    int lx = x + (int)strlen(label) * GFX_CHAR_W;
    gfx_draw_text(lx, y, ok ? "OK" : "FAIL", ok ? COL_OK : COL_FAIL, 1);
}

/* Full status screen. Static results cached in app_init; mv/vol are live. */
static void render_status(int mv, uint8_t vol)
{
    gfx_clear(GFX_BLACK);
    gfx_draw_text(8, 6, "BSP BRING-UP", GFX_WHITE, 2);

    gfx_draw_text(8, 30, "I2C bus:", GFX_WHITE, 1);
    draw_status(8, 44, "  GAUGE 0x36 ", s_gauge_ok);
    draw_status(8, 56, "  EXP   0x38 ", s_exp_ok);
    draw_status(8, 68, "  DAC   0x4C ", s_dac_ok);

    draw_status(8, 86, "I2S bus:     ", s_i2s_ok);

    gfx_draw_text(8, 104, "ADC pot:", s_adc_ok ? GFX_WHITE : COL_FAIL, 1);
    if (s_adc_ok) {
        char buf[24];
        snprintf(buf, sizeof buf, "  mV %4d  vol 0x%02X", mv, vol);
        gfx_draw_text(8, 118, buf, GFX_WHITE, 1);
    }
    gfx_flush();
}
/* ===== END TEST CODE ===== */

void app_init(void)
{
    /* Self-hold power: drive EnableReg (GPIO4) HIGH before anything else.
       The regulator is only latched on by USB/the power button at boot; if the
       ESP32 doesn't take over the hold immediately, releasing them cuts power.
       Preload the output register high, THEN enable the driver, so the pin never
       drives low (even briefly) on its way up. */
    gpio_set_level(PIN_REG_EN, 1);
    gpio_set_direction(PIN_REG_EN, GPIO_MODE_OUTPUT);

    /* ===== TEST CODE — bsp bring-up. The display must come up first (it shows the
       results); the other buses are probed with soft error handling so a failure
       reports FAIL on screen instead of aborting the boot. ===== */
    spi_device_handle_t disp_spi;
    ESP_ERROR_CHECK(spi_bus_display_init(&disp_spi));
    ESP_ERROR_CHECK(display_oled_init(disp_spi));

    if (i2c_bus_init(&s_i2c) == ESP_OK) {
        i2c_bus_scan(s_i2c);  /* logs every ACKing address to the console */
        s_gauge_ok = i2c_master_probe(s_i2c, ADDR_GAUGE, 50) == ESP_OK;
        s_exp_ok   = i2c_master_probe(s_i2c, ADDR_EXP,   50) == ESP_OK;
        s_dac_ok   = i2c_master_probe(s_i2c, ADDR_DAC,   50) == ESP_OK;
    }

    s_i2s_ok = i2s_bus_init(&s_i2s) == ESP_OK;  /* bus mounts; no audio without DAC setup */
    s_adc_ok = adc_pot_init(&s_adc) == ESP_OK;

    ESP_LOGI("bsp", "i2c gauge=%d exp=%d dac=%d | i2s=%d | adc=%d",
             s_gauge_ok, s_exp_ok, s_dac_ok, s_i2s_ok, s_adc_ok);
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
    /* ===== TEST CODE — refresh the status screen at ~10 Hz with live pot values.
       Turn the knob: mV and the volume byte should track. ===== */
    for (;;) {
        int mv = 0;
        uint8_t vol = 0;
        if (s_adc_ok && adc_pot_read(s_adc, &mv) == ESP_OK) {
            vol = adc_pot_to_volume(mv);
        }
        render_status(mv, vol);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    /* ===== END TEST CODE ===== */
}
