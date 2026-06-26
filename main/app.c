#include "app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"            /* TEST CODE */
#include "esp_err.h"            /* TEST CODE */
#include "gfx.h"               /* TEST CODE */
#include "spi_bus.h"           /* TEST CODE */
#include "i2c_bus.h"           /* TEST CODE */
#include "sink_i2s_dac.h"      /* TEST CODE */
#include "adc.h"               /* TEST CODE */
#include "audio_dac.h"         /* TEST CODE */
#include "gpio_expander.h"     /* TEST CODE */
#include "fuel_gauge.h"        /* TEST CODE */
#include "display_oled.h"      /* TEST CODE */
#include "root_menu.h"         /* TEST CODE */
#include "driver/i2c_master.h" /* TEST CODE */
#include <stdio.h>             /* TEST CODE */
#include <string.h>            /* TEST CODE */
#include <math.h>              /* TEST CODE */
#include "driver/gpio.h"
#include "board_pins.h"

/* ===== TEST CODE — bsp bring-up. Probes each bus and shows pass/fail on the OLED.
   Remove this whole block once the real drivers/services own the buses. ===== */

#define GAUGE_TEST 0   /* 1: show the fuel-gauge snapshot instead of the BSP status screen */
#define MENU_TEST  0   /* 1: show the root menu instead of the bring-up screens */
#define TONE_TEST  1   /* 1: emit a continuous 440 Hz tone on the wired DAC path and stay there
                          (overrides MENU_TEST/the status loop). Set to 0 for normal boot. */

#define COL_OK   gfx_rgb(0, 220, 0)
#define COL_FAIL gfx_rgb(220, 0, 0)

/* Known I2C devices on the shared bus (see memory hardware-bringup). */
#define ADDR_GAUGE 0x36  /* MAX17260 fuel gauge */
#define ADDR_EXP   0x38  /* PI4IOE5V9554A expander */
#define ADDR_DAC   0x4C  /* PCM5242 DAC */

static i2c_master_bus_handle_t   s_i2c;
static adc_oneshot_unit_handle_t s_adc;

static bool s_gauge_ok, s_exp_ok, s_dac_ok, s_i2s_ok, s_adc_ok, s_dac_drv_ok, s_exp_drv_ok;
static bool s_gauge_drv_ok;

/* Draw "label" in white, then OK (green) / FAIL (red) right after it. */
static void draw_status(int x, int y, const char *label, bool ok)
{
    gfx_draw_text(x, y, label, GFX_WHITE, 1);
    int lx = x + (int)strlen(label) * GFX_CHAR_W;
    gfx_draw_text(lx, y, ok ? "OK" : "FAIL", ok ? COL_OK : COL_FAIL, 1);
}

/* Full status screen. Static results cached in app_init; mv/vol/dac_rd/exp_in are live. */
static void render_status(int mv, uint8_t vol, int dac_rd, int exp_in)
{
    gfx_clear(GFX_BLACK);
    gfx_draw_text(8, 6, "BSP BRING-UP", GFX_WHITE, 2);

    gfx_draw_text(8, 30, "I2C bus:", GFX_WHITE, 1);
    draw_status(8, 42, "  GAUGE 0x36 ", s_gauge_ok);
    draw_status(8, 52, "  EXP   0x38 ", s_exp_ok);
    draw_status(8, 62, "  DAC   0x4C ", s_dac_ok);

    draw_status(8, 78, "I2S bus:     ", s_i2s_ok);

    gfx_draw_text(8, 94, "ADC pot:", s_adc_ok ? GFX_WHITE : COL_FAIL, 1);
    if (s_adc_ok) {
        char buf[24];
        snprintf(buf, sizeof buf, "  mV %4d  vol 0x%02X", mv, vol);
        gfx_draw_text(8, 106, buf, GFX_WHITE, 1);
    }

    /* DAC driver: wrote 'vol', read 'dac_rd' back over I2C; match = link OK. */
    draw_status(8, 124, "DAC drv:     ", s_dac_drv_ok && dac_rd == vol);
    if (s_dac_drv_ok) {
        char buf[24];
        snprintf(buf, sizeof buf, "  wr 0x%02X rd 0x%02X", vol, dac_rd);
        gfx_draw_text(8, 136, buf, GFX_WHITE, 1);
    }

    /* Expander: live input port; press a button and watch its bit change. */
    draw_status(8, 154, "EXP drv:     ", s_exp_drv_ok);
    if (s_exp_drv_ok) {
        char buf[24];
        snprintf(buf, sizeof buf, "  in 0x%02X", exp_in);
        gfx_draw_text(8, 166, buf, GFX_WHITE, 1);
    }
    
    gfx_flush();
}

/* Dump the fuel-gauge snapshot. 'rd' is the result of fuel_gauge_read(). */
static void render_gauge(const fuel_gauge_data_t *d, esp_err_t rd)
{
    gfx_clear(GFX_BLACK);
    gfx_draw_text(8, 6, "FUEL GAUGE", GFX_WHITE, 2);

    if (rd != ESP_OK) {
        gfx_draw_text(8, 40, "READ FAIL", COL_FAIL, 1);
        gfx_flush();
        return;
    }

    char buf[28];
    int y = 30;
    snprintf(buf, sizeof buf, "SoC   %5.1f %%",  d->soc_pct);     gfx_draw_text(8, y, buf, GFX_WHITE, 1); y += 14;
    snprintf(buf, sizeof buf, "Volt  %5.3f V",   d->voltage_v);   gfx_draw_text(8, y, buf, GFX_WHITE, 1); y += 14;
    snprintf(buf, sizeof buf, "Curr  %6.1f mA",  d->current_ma);  gfx_draw_text(8, y, buf, GFX_WHITE, 1); y += 14;
    snprintf(buf, sizeof buf, "Cap   %5.0f mAh", d->rep_cap_mah); gfx_draw_text(8, y, buf, GFX_WHITE, 1); y += 14;
    snprintf(buf, sizeof buf, "TTE   %6.0f s",   d->tte_s);       gfx_draw_text(8, y, buf, GFX_WHITE, 1); y += 14;
    snprintf(buf, sizeof buf, "TTF   %6.0f s",   d->ttf_s);       gfx_draw_text(8, y, buf, GFX_WHITE, 1); y += 14;
    snprintf(buf, sizeof buf, "Age   %5.1f %%",  d->age_pct);     gfx_draw_text(8, y, buf, GFX_WHITE, 1); y += 14;
    snprintf(buf, sizeof buf, "Temp  %5.1f C",   d->die_temp_c);  gfx_draw_text(8, y, buf, GFX_WHITE, 1); y += 14;
    snprintf(buf, sizeof buf, "Cycl  %5.2f",     d->cycles);      gfx_draw_text(8, y, buf, GFX_WHITE, 1);

    gfx_flush();
}

#if TONE_TEST
/* Re-validate the wired output path (I2S clocking + PCM5242 PLL lock + analog mute/amp
   sequencing) by streaming a continuous 440 Hz sine through the audio_sink_t. Assumes
   app_init() already ran sink_i2s_dac_init(), audio_dac_init() and gpio_expander_init().
   Never returns. */
#define TONE_FS      44100              /* I2S bus default rate (see i2s_bus_init) */
#define TONE_FREQ    441                /* 100 samples/period at 44100 -> seamless table loop */
#define TONE_FRAMES  ((TONE_FS / TONE_FREQ) * 10)  /* 1000 frames = exactly 10 periods */
#define TONE_VOL_R   0x68               /* right-channel volume byte (~ -20 dB), the louder analog side */
#define TONE_VOL_L   0x48               /* left boosted ~8 dB (lower byte = louder) to offset the imbalance; tune by ear */

/* Sanity readback: dump the volume-link mode (PCTL, 0x3C) and both channel volume
   registers (L 0x3D, R 0x3E) straight off the DAC. With independent L/R, PCTL should
   read 0x00 and both volume bytes should match what audio_dac_set_volume() wrote. Uses a
   throwaway device handle so it needs nothing from the driver. */
static void dac_dump_volume_regs(void)
{
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = ADDR_DAC,
        .scl_speed_hz    = I2C_BUS_FREQ_HZ,
    };
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(s_i2c, &cfg, &dev) != ESP_OK) {
        ESP_LOGW("tone", "diag: could not add DAC device handle");
        return;
    }
    uint8_t pctl = 0xFF, vl = 0xFF, vr = 0xFF, reg;
    reg = 0x3C; i2c_master_transmit_receive(dev, &reg, 1, &pctl, 1, 100);
    reg = 0x3D; i2c_master_transmit_receive(dev, &reg, 1, &vl,   1, 100);
    reg = 0x3E; i2c_master_transmit_receive(dev, &reg, 1, &vr,   1, 100);
    ESP_LOGI("tone", "PCTL(0x3C)=0x%02X  VOL_L(0x3D)=0x%02X  VOL_R(0x3E)=0x%02X "
                     "(PCTL 0x00 = independent L/R)", pctl, vl, vr);

    i2c_master_bus_rm_device(dev);
}

static void audio_tone_test(void)
{
    const audio_sink_t *sink = sink_i2s_dac_get();

    /* Bring the wired path up through the sink: I2S rate + DAC PLL lock + standby/mute off
       + pop-free XSMT-then-amp sequencing all live in i2s_start() now. */
    sink->start(TONE_FS, 16, 2);
    sink->set_volume(TONE_VOL_L, TONE_VOL_R);  /* left boosted to balance the louder right analog channel */

    /* Precompute one whole number of sine periods ONCE. The LX6 FPU is single-precision
       only, so double sin() is software-emulated and far too slow to run per sample in the
       streaming loop (starves the I2S DMA -> choppy audio). Doing it once is fine. */
    static int16_t buf[TONE_FRAMES * 2];   /* stereo, identical L/R */
    const int period = TONE_FS / TONE_FREQ;
    for (int i = 0; i < TONE_FRAMES; i++) {
        int16_t s = (int16_t)(sin(2.0 * 3.14159265358979 * (i % period) / period) * 10000.0);
        buf[2 * i]     = s;    /* L */
        buf[2 * i + 1] = s;    /* R */
    }

    uint8_t clk = 0xFF;
    audio_dac_get_clock_status(&clk);
    ESP_LOGI("tone", "DAC clock status 0x%02X (0x40 = PLL locked, SCK grounded by design)", clk);
    dac_dump_volume_regs();   /* confirm the independent L/R volume bytes */

    for (;;) {
        size_t written;
        sink->write(buf, sizeof buf, &written);  /* loops seamlessly */
    }
}
#endif /* TONE_TEST */
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

    s_i2s_ok = sink_i2s_dac_init() == ESP_OK;  /* sink owns the I2S bus; no audio without DAC setup */
    s_adc_ok = adc_pot_init(&s_adc) == ESP_OK;
    s_dac_drv_ok = (s_i2c != NULL) && audio_dac_init(s_i2c) == ESP_OK;
    s_exp_drv_ok = (s_i2c != NULL) && gpio_expander_init(s_i2c) == ESP_OK;
    s_gauge_drv_ok = (s_i2c != NULL) && fuel_gauge_init(s_i2c) == ESP_OK;

    ESP_LOGI("bsp", "i2c gauge=%d exp=%d dac=%d | i2s=%d | adc=%d | dac_drv=%d | exp_drv=%d | gauge_drv=%d",
             s_gauge_ok, s_exp_ok, s_dac_ok, s_i2s_ok, s_adc_ok, s_dac_drv_ok, s_exp_drv_ok, s_gauge_drv_ok);
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
#if TONE_TEST
    /* ===== TEST CODE — tone test takes over the boot; never returns. ===== */
    audio_tone_test();
#endif

    /* ===== TEST CODE — show the root menu. The display is already up from
       app_init(). The menu is static, so render once and idle (no redraw).
       Set MENU_TEST 0 to fall back to the bring-up screens below. ===== */
    if (MENU_TEST) {
        screen_t *s = root_menu();
        s->render(s);
        gfx_flush();
        for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
    }
    /* ===== END TEST CODE ===== */

    /* ===== TEST CODE — refresh the status screen at ~10 Hz with live pot values.
       Turn the knob: mV and the volume byte should track. ===== */
    for (;;) {
        int mv = 0;
        uint8_t vol = 0;
        if (s_adc_ok && adc_pot_read(s_adc, &mv) == ESP_OK) {
            vol = adc_pot_to_volume(mv);
        }

        /* Push the pot volume to the DAC over I2C and read it back to confirm the link. */
        int dac_rd = -1;
        if (s_dac_drv_ok) {
            uint8_t rd;
            audio_dac_set_volume(vol, vol);
            if (audio_dac_get_volume(&rd) == ESP_OK) dac_rd = rd;
        }

        /* Read all expander inputs; pressing a button flips its bit. */
        int exp_in = -1;
        if (s_exp_drv_ok) {
            uint8_t in;
            if (gpio_expander_read_all(&in) == ESP_OK) exp_in = in;
        }

        if (GAUGE_TEST) {
            fuel_gauge_data_t d;
            esp_err_t rd = s_gauge_drv_ok ? fuel_gauge_read(&d) : ESP_FAIL;
            render_gauge(&d, rd);
        } else {
            render_status(mv, vol, dac_rd, exp_in);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    /* ===== END TEST CODE ===== */
}
