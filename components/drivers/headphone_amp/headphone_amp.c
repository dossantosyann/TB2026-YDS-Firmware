#include "headphone_amp.h"
#include "gpio_expander.h"
#include "board_pins.h"

esp_err_t headphone_amp_enable(bool enable)
{
    /* MAX97220 SHDN (expander IO1) is active-low: high = on, low = shutdown. */
    return gpio_expander_set(EXP_AMP_SHDN, enable);
}
