#include "led_task.h"
#include "shared_state.h"
#include "app_config.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ── LED task ─────────────────────────────────────────────────────────────── */

static void led_task(void *arg)
{
    (void)arg;

    /* Configure GPIO2 as output, no pull */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(LED_GPIO, 1); /* start OFF (active LOW) */

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(LED_TICK_MS));

        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        led_state_t state = g_led_state;

        /* CLIENT_IDLE → CLIENT_DATA if car data arrived recently */
        if (state == LED_STATE_CLIENT_IDLE) {
            uint32_t age = now_ms - g_last_uart_rx_ms;
            if (age < LED_DATA_TIMEOUT_MS) {
                state = LED_STATE_CLIENT_DATA;
            }
        }

        int level;
        switch (state) {
        case LED_STATE_CONNECTING:
            /* Rapid flash 80 ms */
            level = (now_ms / LED_FLASH_CONN_MS) % 2;
            break;

        case LED_STATE_READY:
            /* Slow blink 500 ms */
            level = (now_ms / LED_BLINK_READY_MS) % 2;
            break;

        case LED_STATE_CLIENT_IDLE:
            /* Fast blink 150 ms */
            level = (now_ms / LED_BLINK_IDLE_MS) % 2;
            break;

        case LED_STATE_CLIENT_DATA:
        default:
            /* Solid ON (active LOW → 0) */
            level = 0;
            break;
        }

        gpio_set_level(LED_GPIO, level);
    }
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void led_task_start(void)
{
    xTaskCreate(led_task, "led", STACK_LED, NULL, PRI_LED, NULL);
}
