#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "shared_state.h"
#include "app_config.h"
#include "uart_driver.h"
#include "wifi_manager.h"
#include "tcp_server.h"
#include "http_server.h"
#include "ws_server.h"
#include "led_task.h"

static const char *TAG = "main";

/* ── Global state definitions (declared extern in shared_state.h) ─────────── */

QueueHandle_t    g_line_queue_tcp    = NULL;
QueueHandle_t    g_line_queue_ws     = NULL;
QueueHandle_t    g_cmd_queue         = NULL;
SemaphoreHandle_t g_ws_clients_mutex = NULL;

volatile led_state_t g_led_state       = LED_STATE_CONNECTING;
volatile uint32_t    g_last_uart_rx_ms = 0;
volatile bool        g_tcp_has_client  = false;
volatile bool        g_ws_has_client   = false;
volatile bool        g_wifi_is_sta     = false;

/* ── Entry point ──────────────────────────────────────────────────────────── */

void app_main(void)
{
    /* 1. Create inter-task queues BEFORE uart_driver_init() because
     *    uart_task starts immediately and accesses g_cmd_queue */
    g_line_queue_tcp    = xQueueCreate(Q_LINE_DEPTH, sizeof(uart_line_t));
    g_line_queue_ws     = xQueueCreate(Q_LINE_DEPTH, sizeof(uart_line_t));
    g_cmd_queue         = xQueueCreate(Q_CMD_DEPTH,  sizeof(tcp_cmd_t));
    g_ws_clients_mutex  = xSemaphoreCreateMutex();

    if (!g_line_queue_tcp || !g_line_queue_ws ||
        !g_cmd_queue       || !g_ws_clients_mutex) {
        uart_send_str("# FATAL: queue/semaphore creation failed!\r\n");
        /* Halt — nothing useful can run without these */
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    /* 2. Initialise UART (starts uart_task which needs queues above) */
    uart_driver_init();
    uart_send_str("\r\n# -- Umbreon WiFi Bridge (RTOS) --\r\n");
    uart_send_str("# Board: Wemos D1 Mini / ESP8266 RTOS SDK\r\n");

    /* 3. Start LED task — shows CONNECTING state during WiFi init */
    g_led_state = LED_STATE_CONNECTING;
    led_task_start();

    /* 4. Initialise WiFi (blocks until connected or AP fallback) */
    uart_send_str("# Connecting to WiFi...\r\n");
    wifi_manager_init();

    /* 5. Print WiFi status over UART */
    char status[160];
    wifi_manager_get_status(status, sizeof(status));
    uart_send_str(status);

    /* 6. Print port assignments */
    uart_send_str("# TCP:  23\r\n");
    uart_send_str("# HTTP: 80\r\n");
    uart_send_str("# WS:   81\r\n");

    /* 7. Transition LED to READY */
    g_led_state = LED_STATE_READY;

    /* 8. Start server tasks */
    tcp_server_start();
    http_server_start();
    ws_server_start();

    uart_send_str("# Status: ready\r\n");

    ESP_LOGI(TAG, "All tasks started — app_main exiting");
    /* app_main returns → FreeRTOS idle task continues */
}
