#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

/* ── Inter-task message types ──────────────────────────────────────────────── */

/* A complete text line from UART, ready to broadcast to network clients */
typedef struct {
    char     data[UART_LINE_SIZE];
    uint16_t len;
} uart_line_t;

/* A command line from a network client, to be forwarded to UART */
typedef struct {
    char     data[TCP_CMD_SIZE];
    uint16_t len;
} tcp_cmd_t;

/* ── LED state ─────────────────────────────────────────────────────────────── */
typedef enum {
    LED_STATE_CONNECTING = 0,   /* rapid flash — connecting to WiFi   */
    LED_STATE_READY,            /* slow blink  — no clients           */
    LED_STATE_CLIENT_IDLE,      /* fast blink  — client, no car data  */
    LED_STATE_CLIENT_DATA,      /* solid ON    — car data flowing      */
} led_state_t;

/* ── Global handles (defined in main.c) ────────────────────────────────────── */

/* Broadcast queues: uart_driver → consumers */
extern QueueHandle_t g_line_queue_tcp;  /* uart_line_t, depth Q_LINE_DEPTH */
extern QueueHandle_t g_line_queue_ws;   /* uart_line_t, depth Q_LINE_DEPTH */

/* Command queue: TCP/WS servers → uart_driver */
extern QueueHandle_t g_cmd_queue;       /* tcp_cmd_t,   depth Q_CMD_DEPTH  */

/* WebSocket client registry mutex */
extern SemaphoreHandle_t g_ws_clients_mutex;

/* LED state (volatile — written by any task, read by led_task) */
extern volatile led_state_t g_led_state;
extern volatile uint32_t    g_last_uart_rx_ms;  /* xTaskGetTickCount()*portTICK_PERIOD_MS */

/* Client presence flags (volatile — written by server tasks, read by each other) */
extern volatile bool g_tcp_has_client;
extern volatile bool g_ws_has_client;

/* WiFi mode flag (set once by wifi_manager, read for #WIFISTATUS) */
extern volatile bool g_wifi_is_sta;
