#include "tcp_server.h"
#include "shared_state.h"
#include "app_config.h"
#include "wifi_manager.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>

static const char *TAG = "tcp";

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static void update_led(bool connected)
{
    g_tcp_has_client = connected;
    if (connected) {
        g_led_state = LED_STATE_CLIENT_IDLE;
    } else {
        g_led_state = (g_ws_has_client) ? LED_STATE_CLIENT_IDLE : LED_STATE_READY;
    }
}

/* Send a string to the TCP client.  Returns -1 on error. */
static int tcp_send(int fd, const char *s, int len)
{
    return send(fd, s, len < 0 ? (int)strlen(s) : len, 0);
}

/* ── Server task ──────────────────────────────────────────────────────────── */

static void tcp_server_task(void *arg)
{
    (void)arg;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        ESP_LOGE(TAG, "socket() failed: %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(TCP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind() failed: %d", errno);
        close(server_fd);
        vTaskDelete(NULL);
        return;
    }
    listen(server_fd, 1);
    ESP_LOGI(TAG, "Listening on port %d", TCP_PORT);

    for (;;) {
        /* Block until a client connects */
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            ESP_LOGW(TAG, "accept() error: %d", errno);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        int nd = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nd, sizeof(nd));
        ESP_LOGI(TAG, "Client connected, fd=%d", client_fd);

        /* Send welcome banner */
        char banner[160];
        char status[128];
        wifi_manager_get_status(status, sizeof(status));
        /* Extract just the IP line from status for the banner */
        char ip_str[32] = "?";
        const char *ip_p = strstr(status, "# IP:    ");
        if (ip_p) {
            ip_p += 9;
            int i = 0;
            while (*ip_p && *ip_p != '\r' && *ip_p != '\n' && i < 31)
                ip_str[i++] = *ip_p++;
            ip_str[i] = '\0';
        }
        int blen = snprintf(banner, sizeof(banner),
            "# Umbreon WiFi Bridge \xe2\x80\x94 connected\r\n"
            "# IP:   %s\r\n"
            "# Mode: %s\r\n",
            ip_str, g_wifi_is_sta ? "STA" : "AP");
        tcp_send(client_fd, banner, blen);

        update_led(true);

        /* ── Serve client ─────────────────────────────────────────────────── */
        char cmd_buf[TCP_CMD_SIZE];
        int  cmd_pos = 0;

        for (;;) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(client_fd, &rfds);

            struct timeval tv = { .tv_sec = 0, .tv_usec = 10000 }; /* 10ms */
            int sel = select(client_fd + 1, &rfds, NULL, NULL, &tv);
            if (sel < 0) {
                ESP_LOGW(TAG, "select() error: %d", errno);
                break;
            }

            if (sel > 0 && FD_ISSET(client_fd, &rfds)) {
                char rx_tmp[128];
                int n = recv(client_fd, rx_tmp, sizeof(rx_tmp), 0);
                if (n <= 0) {
                    ESP_LOGI(TAG, "Client disconnected (recv=%d)", n);
                    break;
                }
                for (int i = 0; i < n; i++) {
                    char c = rx_tmp[i];
                    if (c == '\n') {
                        if (cmd_pos > 0) {
                            tcp_cmd_t cmd;
                            cmd.len = (uint16_t)cmd_pos;
                            memcpy(cmd.data, cmd_buf, cmd_pos);
                            cmd.data[cmd_pos] = '\0';
                            xQueueSend(g_cmd_queue, &cmd, 0);
                            cmd_pos = 0;
                        }
                    } else if (c != '\r' && cmd_pos < TCP_CMD_SIZE - 1) {
                        cmd_buf[cmd_pos++] = c;
                    }
                }
            }

            /* Drain broadcast queue — send telemetry to client */
            uart_line_t msg;
            while (xQueueReceive(g_line_queue_tcp, &msg, 0) == pdTRUE) {
                tcp_send(client_fd, msg.data, msg.len);
                tcp_send(client_fd, "\r\n", 2);
            }
        }

        close(client_fd);
        update_led(false);
        cmd_pos = 0;
        ESP_LOGI(TAG, "Client fd=%d closed", client_fd);
    }
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void tcp_server_start(void)
{
    xTaskCreate(tcp_server_task, "tcp", STACK_TCP, NULL, PRI_TCP, NULL);
}
