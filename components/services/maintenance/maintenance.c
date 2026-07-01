/**
 * @file maintenance.c
 * @brief Maintenance task: periodically runs background chores (currently power_tick).
 * @ingroup services_maintenance
 */
#include "maintenance.h"
#include "power.h"
#include "volume.h"
#include "player.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAINT_TASK_PRIO   3      /* below input (5) and the audio task (22) */
#define MAINT_TASK_STACK  3072   /* fuel_gauge_read does a burst of I2C reads */
#define MAINT_VOL_MS      100    /* volume knob poll rate */
#define MAINT_PWR_DIV     20    /* power_tick every 2000 ms */

static TaskHandle_t s_task;

static void maintenance_task(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(MAINT_VOL_MS);
    int pwr = 0;
    for (;;) {
        volume_poll(NULL, NULL);
        player_poll();
        if (++pwr >= MAINT_PWR_DIV) { power_tick(); pwr = 0; }
        vTaskDelay(period);
    }
}

esp_err_t maintenance_init(void)
{
    if (s_task) return ESP_OK;   /* idempotent */
    if (xTaskCreate(maintenance_task, "maint", MAINT_TASK_STACK, NULL,
                    MAINT_TASK_PRIO, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
