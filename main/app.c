#include "app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_init(void) {}

void app_run(void)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
