#include "settings.h"

esp_err_t settings_init(void)
{
    return ESP_OK;
}

audio_output_t settings_get_output(void)
{
    return AUDIO_OUTPUT_JACK;
}

esp_err_t settings_set_output(audio_output_t output)
{
    return ESP_OK;
}
