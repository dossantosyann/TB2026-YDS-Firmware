#pragma once
/* Minimal host stand-in for ESP-IDF's esp_adc/adc_oneshot.h: adc.h only needs the opaque
   handle type in its prototypes; the host tests never touch the real ADC. */

typedef struct adc_oneshot_unit_ctx_t *adc_oneshot_unit_handle_t;
