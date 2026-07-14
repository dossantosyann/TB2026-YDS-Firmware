/**
 * @file input.c
 * @brief Input service: ISR-notified task reads buttons over I2C/GPIO, debounces, queues events.
 * @ingroup services_input
 */
#include "input.h"
#include "input_logic.h"
#include "gpio_expander.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "diag.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "input";

#define INPUT_TASK_PRIO   5      /* UI/maintenance band, below the audio task (22) */
#define INPUT_TASK_STACK  3072
#define INPUT_QUEUE_LEN   8      /* far more than a human can outrun; events drop if ever full */

static QueueHandle_t    s_queue;
static TaskHandle_t     s_task;
static input_debounce_t s_debounce;   /* zero-init: prev released, no prior press timestamps */
static bool             s_ready;
static volatile uint32_t s_last_activity_ms;   /* time of the last emitted event; drives input_idle_ms */

/* Both pins share this handler: it only wakes the task (I2C/GPIO reads are forbidden in ISR
   context). One notification is enough — the task always re-reads every source on wake. */
static void IRAM_ATTR input_isr(void *arg)
{
    (void)arg;
    BaseType_t hpw = pdFALSE;
    vTaskNotifyGiveFromISR(s_task, &hpw);
    portYIELD_FROM_ISR(hpw);
}

static void input_task(void *arg)
{
    (void)arg;
    diag_register_task(INPUT_TASK_STACK);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);    /* idle here: no polling, no I2C at rest */

        uint8_t port;
        if (gpio_expander_read_all(&port) != ESP_OK) {   /* the read also acks the expander INT */
            ESP_LOGW(TAG, "expander read failed; dropping this interrupt");
            continue;
        }
        bool a_pressed = gpio_get_level(PIN_BTN_A) != 0;  /* A is active-HIGH (others active-low) */
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

        ui_event_t evs[INPUT_BTN_COUNT];
        int n = input_logic_update(&s_debounce, port, a_pressed, now_ms, evs, INPUT_BTN_COUNT);
        if (n > 0) s_last_activity_ms = now_ms;   /* real user action: reset the idle clock */
        for (int i = 0; i < n; i++) {
            xQueueSend(s_queue, &evs[i], 0);
        }
    }
}

esp_err_t input_init(void)
{
    if (s_ready) return ESP_OK;   /* idempotent */

    s_queue = xQueueCreate(INPUT_QUEUE_LEN, sizeof(ui_event_t));
    if (!s_queue) return ESP_ERR_NO_MEM;

    /* Create the task before enabling any interrupt so s_task is valid when the ISR first fires. */
    if (xTaskCreate(input_task, "input", INPUT_TASK_STACK, NULL, INPUT_TASK_PRIO, &s_task) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ESP_OK;

    /* Expander INT (GPIO35): open-drain active-low, asserts on any input change and clears when
       the port is read, so a falling edge marks every press AND release. GPIO34..39 have no
       internal pull; an external pull-up holds the line high at rest. */
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_EXPANDER_INT,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    if ((err = gpio_config(&io)) != ESP_OK) goto fail;

    /* Button A (GPIO39): active-high on its own line. ANYEDGE, not just the press edge: the task
       must observe the release too, or a held-then-released A leaves the debounce state "pressed"
       and the next press is missed. GPIO39 is an RTC-IO, so this config is already wake-compatible;
       no internal pull exists, an external pull-down holds it low at rest. */
    io.pin_bit_mask = 1ULL << PIN_BTN_A;
    io.intr_type    = GPIO_INTR_ANYEDGE;
    if ((err = gpio_config(&io)) != ESP_OK) goto fail;

    /* TODO(sleep): the future sleep service arms A as the ext0 wake source here, e.g.
       esp_sleep_enable_ext0_wakeup(INPUT_WAKE_GPIO, INPUT_WAKE_ACTIVE_LEVEL) (+ rtc_gpio_* as
       needed). This service never calls esp_sleep_*. */

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) goto fail;  /* tolerate a prior install */

    if ((err = gpio_isr_handler_add(PIN_EXPANDER_INT, input_isr, NULL)) != ESP_OK) goto fail;
    if ((err = gpio_isr_handler_add(PIN_BTN_A, input_isr, NULL)) != ESP_OK) goto fail;

    s_last_activity_ms = (uint32_t)(esp_timer_get_time() / 1000);   /* idle starts at 0 */
    s_ready = true;
    return ESP_OK;

fail:
    vTaskDelete(s_task);
    s_task = NULL;
    vQueueDelete(s_queue);
    s_queue = NULL;
    return err;
}

bool input_get_event(ui_event_t *out, TickType_t ticks_to_wait)
{
    if (!s_queue) return false;
    return xQueueReceive(s_queue, out, ticks_to_wait) == pdTRUE;
}

uint32_t input_idle_ms(void)
{
    if (!s_ready) return 0;
    return (uint32_t)(esp_timer_get_time() / 1000) - s_last_activity_ms;
}

void input_get_diag(input_diag_t *out)
{
    /* Lock-free read of the input task's state (display-only; a torn value is harmless). */
    for (int i = 0; i < INPUT_BTN_COUNT; i++) {
        out->pressed[i] = (s_debounce.prev >> i) & 1u;
        out->count[i]   = s_debounce.count[i];
    }
}
