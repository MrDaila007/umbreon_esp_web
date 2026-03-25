#include "led_task.h"
#include "shared_state.h"
#include "app_config.h"
#include "led_logic.h"

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
        int level = led_compute_level(g_led_state, now_ms, g_last_uart_rx_ms);
        gpio_set_level(LED_GPIO, level);
    }
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void led_task_start(void)
{
    xTaskCreate(led_task, "led", STACK_LED, NULL, PRI_LED, NULL);
}
