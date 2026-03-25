#pragma once

/* ── Server ports ──────────────────────────────────────────────────────────── */
#define TCP_PORT        23
#define HTTP_PORT       80
#define WS_PORT         81

/* ── UART ──────────────────────────────────────────────────────────────────── */
#define UART_NUM        UART_NUM_0
#define UART_BAUD       115200

/* ── LED ───────────────────────────────────────────────────────────────────── */
#define LED_GPIO        2   /* GPIO2, active LOW (Wemos D1 Mini built-in LED) */

/* ── Line/frame buffers ────────────────────────────────────────────────────── */
#define UART_RING_SIZE  1024    /* internal UART driver ring buffer */
#define UART_LINE_SIZE  512     /* max single UART → network line   */
#define TCP_CMD_SIZE    256     /* max TCP/WS → UART command line   */
#define WS_FRAME_SIZE   640     /* max WS outbound text frame       */
#define HTTP_CHUNK_SIZE 1024    /* HTTP response streaming chunk    */

/* ── WebSocket ─────────────────────────────────────────────────────────────── */
#define WS_MAX_CLIENTS  4
#define WS_GUID         "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_HS_TIMEOUT_MS 5000  /* handshake read timeout */

/* ── FreeRTOS task stacks (bytes) ──────────────────────────────────────────── */
#define STACK_UART      2048
#define STACK_TCP       3072
#define STACK_HTTP      3072
#define STACK_WS        4096
#define STACK_LED       1024    /* needs room for gpio_config() + ESP_LOG printf */

/* ── FreeRTOS task priorities ──────────────────────────────────────────────── */
#define PRI_UART        10
#define PRI_WS          7
#define PRI_TCP         6
#define PRI_HTTP        5
#define PRI_LED         3

/* ── Queue depths ──────────────────────────────────────────────────────────── */
#define Q_LINE_DEPTH    8   /* uart_line_t items per broadcast queue */
#define Q_CMD_DEPTH     4   /* tcp_cmd_t items in command queue      */

/* ── WiFi timing (ms) ──────────────────────────────────────────────────────── */
#define STA_TIMEOUT_MS  15000   /* per-network; worst case = 2 × 15s + AP */
#define RECONNECT_MS    5000

/* ── LED timing (ms) ───────────────────────────────────────────────────────── */
#define LED_FLASH_CONN_MS   80   /* rapid flash: connecting           */
#define LED_BLINK_READY_MS  500  /* slow blink:  ready, no client     */
#define LED_BLINK_IDLE_MS   150  /* fast blink:  client, no car data  */
#define LED_DATA_TIMEOUT_MS 500  /* solid-ON window after last RX     */
#define LED_TICK_MS         10   /* LED task wakeup interval          */

/* ── AP defaults (overridable in wifi_config.h) ────────────────────────────── */
#ifndef AP_SSID
#define AP_SSID "Umbreon"
#endif
#ifndef AP_PASS
#define AP_PASS "12345678"
#endif
