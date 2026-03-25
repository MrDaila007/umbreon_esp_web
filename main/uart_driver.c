#include "uart_driver.h"
#include "shared_state.h"
#include "app_config.h"
#include "wifi_manager.h"

#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <string.h>
#include <stdint.h>

static const char *TAG = "uart";

/* Internal UART event queue created by uart_driver_install() */
static QueueHandle_t s_uart_event_queue;

/* Line accumulation buffer (UART → network) */
static char     s_uart_line[UART_LINE_SIZE];
static uint16_t s_uart_pos = 0;

/* ── Internal helpers ─────────────────────────────────────────────────────── */

/* Forward a complete line to both broadcast queues */
static void dispatch_line(const char *line, uint16_t len)
{
    uart_line_t msg;
    if (len >= UART_LINE_SIZE) len = UART_LINE_SIZE - 1;
    memcpy(msg.data, line, len);
    msg.data[len] = '\0';
    msg.len = len;

    /* Non-blocking sends — drop if queues are full to avoid blocking UART RX */
    xQueueSend(g_line_queue_tcp, &msg, 0);
    xQueueSend(g_line_queue_ws,  &msg, 0);
}

/* Handle one complete UART line (without trailing \r\n) */
static void handle_line(const char *line, uint16_t len)
{
    /* Update last-RX timestamp for LED state machine */
    g_last_uart_rx_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /* Intercept #WIFISTATUS — reply to Pico, do NOT forward to clients */
    if (len == 11 && memcmp(line, "#WIFISTATUS", 11) == 0) {
        char status[160];
        wifi_manager_get_status(status, sizeof(status));
        uart_send_str(status);
        return;
    }

    dispatch_line(line, len);
}

/* Drain the command queue and write pending commands to UART.
 * Some commands are intercepted and handled by the bridge itself. */
static void drain_cmd_queue(void)
{
    tcp_cmd_t cmd;
    while (xQueueReceive(g_cmd_queue, &cmd, 0) == pdTRUE) {
        /* Intercept $RSSI — respond with WiFi signal strength */
        if (cmd.len == 5 && memcmp(cmd.data, "$RSSI", 5) == 0) {
            int rssi = wifi_manager_get_rssi();
            char resp[32];
            int rlen = snprintf(resp, sizeof(resp), "$RSSI:%d", rssi);
            dispatch_line(resp, (uint16_t)rlen);
            continue;
        }
        uart_write_bytes(UART_NUM, cmd.data, cmd.len);
        uart_write_bytes(UART_NUM, "\r\n", 2);
    }
}

/* ── UART task ────────────────────────────────────────────────────────────── */

static void uart_task(void *arg)
{
    (void)arg;
    uart_event_t event;

    for (;;) {
        /* Wait for UART event, but wake up every 50ms to drain cmd queue
         * even if the car is silent (no telemetry). */
        if (xQueueReceive(s_uart_event_queue, &event,
                          pdMS_TO_TICKS(50)) != pdTRUE) {
            drain_cmd_queue();
            continue;
        }

        switch (event.type) {
        case UART_DATA: {
            /* Read available bytes from the internal ring buffer */
            uint8_t buf[128];
            int bytes = event.size;
            while (bytes > 0) {
                int chunk = (bytes < (int)sizeof(buf)) ? bytes : (int)sizeof(buf);
                int n = uart_read_bytes(UART_NUM, buf, chunk, 0);
                if (n <= 0) break;
                bytes -= n;

                for (int i = 0; i < n; i++) {
                    char c = (char)buf[i];
                    if (c == '\n') {
                        if (s_uart_pos > 0) {
                            s_uart_line[s_uart_pos] = '\0';
                            handle_line(s_uart_line, s_uart_pos);
                            s_uart_pos = 0;
                        }
                    } else if (c != '\r' && s_uart_pos < UART_LINE_SIZE - 1) {
                        s_uart_line[s_uart_pos++] = c;
                    }
                }
            }
            /* After draining RX, flush pending TX commands */
            drain_cmd_queue();
            break;
        }

        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
            ESP_LOGW(TAG, "UART overflow — flushing");
            uart_flush_input(UART_NUM);
            xQueueReset(s_uart_event_queue);
            s_uart_pos = 0;
            break;

        case UART_FRAME_ERR:
        case UART_PARITY_ERR:
            ESP_LOGW(TAG, "UART error type=%d", event.type);
            break;

        default:
            break;
        }
    }
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void uart_driver_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &cfg));

    /* Install driver with internal ring buffer (2 × UART_RING_SIZE)
     * and an event queue of depth 16. TX buffer disabled (0). */
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM,
                                        UART_RING_SIZE * 2,  /* RX buf */
                                        0,                   /* TX buf (0 = synchronous) */
                                        16,                  /* event queue depth */
                                        &s_uart_event_queue,
                                        0));

    /* Tell VFS to use our driver for UART0 console output.
     * Without this, ESP_LOG conflicts with uart_driver_install. */
    esp_vfs_dev_uart_use_driver(UART_NUM);

    xTaskCreate(uart_task, "uart", STACK_UART, NULL, PRI_UART, NULL);
    ESP_LOGI(TAG, "UART0 @ %d baud, task started", UART_BAUD);
}

void uart_send_str(const char *s)
{
    uart_write_bytes(UART_NUM, s, strlen(s));
}
