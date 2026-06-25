#include "audio_sink.h"
#include "audio_dac.h"
#include "sink_i2s_dac.h"
#include "gpio_expander.h"
#include "board_pins.h"

esp_err_t sink_i2s_dac_output_enable(bool enable)
{
    /* PCM5242 XSMT (expander IO0) is active-low: high = un-mute, low = soft mute. */
    return gpio_expander_set(EXP_DAC_MUTE, enable);
}

esp_err_t sink_i2s_dac_amp_enable(bool enable)
{
    /* MAX97220 SHDN (expander IO1) is active-low: high = on, low = shutdown. */
    return gpio_expander_set(EXP_AMP_SHDN, enable);
}
