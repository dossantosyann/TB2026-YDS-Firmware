#include "app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_init(void)
{
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
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
