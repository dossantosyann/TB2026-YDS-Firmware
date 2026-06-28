/**
 * @file maintenance.c
 * @brief Maintenance task: periodically runs background chores (currently power_tick).
 * @ingroup services_maintenance
 */
#include "maintenance.h"
#include "power.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAINT_TASK_PRIO   3      /* below input (5) and the audio task (22) */
#define MAINT_TASK_STACK  3072   /* fuel_gauge_read does a burst of I2C reads */
#define MAINT_PERIOD_MS   2000   /* battery state changes slowly */

static TaskHandle_t s_task;

static void maintenance_task(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(MAINT_PERIOD_MS);
    for (;;) {
        power_tick();
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
