#pragma once
/*
 * Pure LED state machine logic — no hardware dependencies.
 * Can be unit-tested on host without FreeRTOS or GPIO.
 */

#include "app_config.h"
#include <stdint.h>

#ifndef UNIT_TEST
#include "shared_state.h"
#else
/* Minimal led_state_t for host-side tests */
typedef enum {
    LED_STATE_CONNECTING = 0,
    LED_STATE_READY,
    LED_STATE_CLIENT_IDLE,
    LED_STATE_CLIENT_DATA,
} led_state_t;
#endif

/*
 * Compute the GPIO output level for the status LED.
 *
 * @param state         Current LED state (from g_led_state)
 * @param now_ms        Current time in milliseconds
 * @param last_rx_ms    Timestamp of last UART RX line (from g_last_uart_rx_ms)
 * @return              GPIO level: 0 = LED ON (active low), 1 = LED OFF
 */
static inline int led_compute_level(led_state_t state, uint32_t now_ms, uint32_t last_rx_ms)
{
    /* CLIENT_IDLE -> CLIENT_DATA if car data arrived recently */
    if (state == LED_STATE_CLIENT_IDLE) {
        uint32_t age = now_ms - last_rx_ms;
        if (age < LED_DATA_TIMEOUT_MS) {
            state = LED_STATE_CLIENT_DATA;
        }
    }

    switch (state) {
    case LED_STATE_CONNECTING:
        return (now_ms / LED_FLASH_CONN_MS) % 2;

    case LED_STATE_READY:
        return (now_ms / LED_BLINK_READY_MS) % 2;

    case LED_STATE_CLIENT_IDLE:
        return (now_ms / LED_BLINK_IDLE_MS) % 2;

    case LED_STATE_CLIENT_DATA:
    default:
        return 0; /* solid ON (active LOW) */
    }
}
