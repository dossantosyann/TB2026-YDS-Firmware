/**
 * @file diag.c
 * @brief Diagnostics report: stack occupancy of the registered tasks, FreeRTOS task list
 *        and run-time stats, internal/PSRAM heap usage.
 * @ingroup services_diag
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "diag.h"

#if DIAG_ENABLED

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#if !CONFIG_FREERTOS_USE_TRACE_FACILITY || !CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS || \
    !CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
#error "DIAG_ENABLED needs CONFIG_FREERTOS_USE_TRACE_FACILITY, \
CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS and CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS \
(see the Diagnostics block in sdkconfig.defaults; delete sdkconfig and rebuild to pick them up)."
#endif

static const char *TAG = "diag";

/* The four permanent tasks: ui, audio, input, maintenance. */
#define DIAG_MAX_TASKS 4

typedef struct {
    TaskHandle_t handle;
    uint32_t     stack_bytes;   /* as passed to xTaskCreate (bytes on ESP-IDF) */
} diag_task_t;

static diag_task_t s_tasks[DIAG_MAX_TASKS];
static size_t s_count;
/* Registrations come from four different tasks on both cores; the dump reads the array.
   The critical section is only around the slot claim — the entries themselves are written
   once, at task start, and never mutated afterwards. */
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

void diag_register_task(uint32_t stack_bytes)
{
    taskENTER_CRITICAL(&s_lock);
    size_t i = s_count;
    if (i < DIAG_MAX_TASKS) s_count++;
    taskEXIT_CRITICAL(&s_lock);

    if (i >= DIAG_MAX_TASKS) {
        ESP_LOGW(TAG, "registry full (%d), '%s' not tracked", DIAG_MAX_TASKS, pcTaskGetName(NULL));
        return;
    }
    s_tasks[i].stack_bytes = stack_bytes;
    s_tasks[i].handle      = xTaskGetCurrentTaskHandle();
}

void diag_task_exit(void)
{
    /* No allocated size to compare against: an ephemeral task's stack constant lives at its
       creation site, not here. The high water mark alone is what sizes it. */
    ESP_LOGI(TAG, "task '%s' exiting: stack high water mark %u B free",
             pcTaskGetName(NULL), (unsigned)uxTaskGetStackHighWaterMark(NULL));
}

/* One row of the stack table. hwm is the smallest free stack the task ever had, so
   stack - hwm is its worst-case use since boot. */
static void dump_row(const diag_task_t *t)
{
    uint32_t hwm  = (uint32_t)uxTaskGetStackHighWaterMark(t->handle);
    uint32_t used = t->stack_bytes > hwm ? t->stack_bytes - hwm : 0;
    uint32_t pct  = t->stack_bytes ? (used * 100) / t->stack_bytes : 0;

    BaseType_t core = xTaskGetCoreID(t->handle);   /* tskNO_AFFINITY = free to run on either */
    const char *core_s = core == 0 ? "0" : core == 1 ? "1" : "any";

    ESP_LOGI(TAG, "%-12s %4s %5u %8u %8u %8u %5u%%  %s",
             pcTaskGetName(t->handle), core_s, (unsigned)uxTaskPriorityGet(t->handle),
             (unsigned)t->stack_bytes, (unsigned)hwm, (unsigned)used, (unsigned)pct,
             pct > DIAG_ALERT_PCT ? "<!>" : "");
}

/* vTaskList / vTaskGetRunTimeStats write into a caller-supplied buffer with no bound check.
   IDF documents ~40 chars per task per line; 64 leaves room for long names and wide counters.
   The buffer goes to PSRAM: it is transient, and internal DRAM is the scarce pool here (the
   Bluetooth stack lives in it). */
static char *stats_buf_alloc(size_t *out_size)
{
    size_t n = (size_t)uxTaskGetNumberOfTasks() + 4;   /* margin: tasks may spawn mid-dump */
    size_t size = n * 64 + 128;                        /* + the header lines */
    char *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!buf) buf = heap_caps_malloc(size, MALLOC_CAP_INTERNAL);   /* PSRAM absent/full */
    if (buf) *out_size = size;
    return buf;
}

static void dump_heap(const char *what, uint32_t caps)
{
    ESP_LOGI(TAG, "%-9s free %8u B   min free ever %8u B",
             what, (unsigned)heap_caps_get_free_size(caps),
             (unsigned)heap_caps_get_minimum_free_size(caps));
}

void diag_dump_tasks(void)
{
    ESP_LOGI(TAG, "================ diagnostics ================");
    ESP_LOGI(TAG, "%-12s %4s %5s %8s %8s %8s %6s  %s",
             "task", "core", "prio", "stack", "hwm", "used", "use", "alert");
    for (size_t i = 0; i < s_count; ++i) {
        dump_row(&s_tasks[i]);
    }
    ESP_LOGI(TAG, "(stack/hwm/used in bytes; hwm = smallest free stack ever, used = stack - hwm; "
                  "<!> = over %d%%)", DIAG_ALERT_PCT);

    /* Everything else on the system — the IDF service tasks and the Bluedroid stack, which
       this firmware never creates itself, plus both IDLE tasks (their share of run time is
       the CPU headroom). */
    size_t size;
    char *buf = stats_buf_alloc(&size);
    if (!buf) {
        ESP_LOGW(TAG, "no memory for the FreeRTOS stats buffer, skipping vTaskList/RunTimeStats");
    } else {
        buf[0] = '\0';
        vTaskList(buf);
        ESP_LOGI(TAG, "all tasks (name / state / prio / free stack B / num / core):\n%s", buf);

        buf[0] = '\0';
        vTaskGetRunTimeStats(buf);
        ESP_LOGI(TAG, "run time (name / ticks / %% of total):\n%s", buf);

        heap_caps_free(buf);
    }

    dump_heap("internal", MALLOC_CAP_INTERNAL);
    dump_heap("psram", MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "=============================================");
}

void diag_tick(uint32_t elapsed_ms)
{
    static uint32_t acc;
    acc += elapsed_ms;
    if (acc < DIAG_DUMP_MS) return;
    acc = 0;
    diag_dump_tasks();
}

#endif /* DIAG_ENABLED */
