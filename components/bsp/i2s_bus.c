/**
 * @file i2s_bus.c
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "i2s_bus.h"
#include "board_pins.h"

/* Both 16-bit MP3 and up to 24-bit WAV ride MSB-justified in a fixed 32-bit slot,
   so only the data width and sample rate change per track. */
#define I2S_SLOT_BITS     I2S_SLOT_BIT_WIDTH_32BIT

/* Bus comes up here; the decoder retunes via i2s_bus_reconfig() per file. */
#define I2S_DEFAULT_RATE  (44100)
/* Always 32-bit data to match the fixed 32-bit slot: the decoders expand every sample
   to a 32-bit MSB-justified word before writing, so the DMA copies straight through
   with no implicit expansion that changed behaviour in IDF v6. */
#define I2S_DEFAULT_BITS  (I2S_DATA_BIT_WIDTH_32BIT)

/* Fill a std config for a given rate/width. APLL: the default PLL can't hit the
   44.1 kHz family or 192 kHz exactly (audible clock error); it draws a little more
   power but only while the channel is enabled. */
static i2s_std_config_t std_config(uint32_t rate_hz, i2s_data_bit_width_t bits)
{
    i2s_std_config_t cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(rate_hz),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bits, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,        // PCM5242 derives its clock from BCLK
            .bclk = PIN_I2S_BCLK,           // GPIO26
            .ws   = PIN_I2S_WS,             // GPIO25 (LRCK)
            .dout = PIN_I2S_DOUT,           // GPIO27
            .din  = I2S_GPIO_UNUSED,        // output only
        },
    };
    cfg.clk_cfg.clk_src = I2S_CLK_SRC_APLL;
    cfg.slot_cfg.slot_bit_width = I2S_SLOT_BITS;
    return cfg;
}

esp_err_t i2s_bus_init(i2s_chan_handle_t *out_tx_handle)
{
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    /* Deeper DMA ring: the default 6 desc x 240 frames is only 7.5 ms of cushion at
       192 kHz, so any SD latency spike is an audible underrun. 12 x 511 = 6132 frames
       gives ~32 ms at 192 kHz (~139 ms at 44.1 kHz) for ~49 KB of internal DMA RAM.
       511 frames is the ceiling per descriptor (4092-byte limit / 8-byte frame). */
    chan_cfg.dma_desc_num  = 12;
    chan_cfg.dma_frame_num = 511;
    esp_err_t err = i2s_new_channel(&chan_cfg, out_tx_handle, NULL); // TX only, no RX
    if (err != ESP_OK) return err;

    const i2s_std_config_t cfg = std_config(I2S_DEFAULT_RATE, I2S_DEFAULT_BITS);
    err = i2s_channel_init_std_mode(*out_tx_handle, &cfg);
    if (err != ESP_OK) return err;

    return i2s_channel_enable(*out_tx_handle);
}

esp_err_t i2s_bus_reconfig(i2s_chan_handle_t tx, uint32_t rate_hz, uint8_t bits)
{
    (void)bits;   /* decoders always emit 32-bit samples; data_bit_width stays 32 */
    const i2s_std_config_t cfg = std_config(rate_hz, I2S_DATA_BIT_WIDTH_32BIT);

    esp_err_t err = i2s_channel_disable(tx);    // must be in READY state to retune
    if (err != ESP_OK) return err;
    err = i2s_channel_reconfig_std_clock(tx, &cfg.clk_cfg);
    if (err != ESP_OK) return err;
    err = i2s_channel_reconfig_std_slot(tx, &cfg.slot_cfg);
    if (err != ESP_OK) return err;
    return i2s_channel_enable(tx);
}
