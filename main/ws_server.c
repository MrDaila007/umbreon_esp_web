#include "ws_server.h"
#include "ws_handshake.h"
#include "shared_state.h"
#include "app_config.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <errno.h>

static const char *TAG = "ws";

/* ── Client registry ──────────────────────────────────────────────────────── */

static int s_ws_fds[WS_MAX_CLIENTS];

static void registry_init(void)
{
    for (int i = 0; i < WS_MAX_CLIENTS; i++) s_ws_fds[i] = -1;
}

static void ws_add_client(int fd)
{
    xSemaphoreTake(g_ws_clients_mutex, portMAX_DELAY);
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] == -1) {
            s_ws_fds[i] = fd;
            break;
        }
    }
    /* Count active */
    int count = 0;
    for (int i = 0; i < WS_MAX_CLIENTS; i++) if (s_ws_fds[i] != -1) count++;
    xSemaphoreGive(g_ws_clients_mutex);

    g_ws_has_client = (count > 0);
    g_led_state = LED_STATE_CLIENT_IDLE;
    ESP_LOGI(TAG, "WS client added fd=%d (total=%d)", fd, count);
}

static void ws_remove_client(int fd)
{
    xSemaphoreTake(g_ws_clients_mutex, portMAX_DELAY);
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] == fd) s_ws_fds[i] = -1;
    }
    int count = 0;
    for (int i = 0; i < WS_MAX_CLIENTS; i++) if (s_ws_fds[i] != -1) count++;
    xSemaphoreGive(g_ws_clients_mutex);

    g_ws_has_client = (count > 0);
    if (!g_ws_has_client) {
        g_led_state = g_tcp_has_client ? LED_STATE_CLIENT_IDLE : LED_STATE_READY;
    }
    ESP_LOGI(TAG, "WS client removed fd=%d (total=%d)", fd, count);
}

/* ── Public broadcast ─────────────────────────────────────────────────────── */

void ws_broadcast(const char *text, uint16_t len)
{
    xSemaphoreTake(g_ws_clients_mutex, portMAX_DELAY);
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] == -1) continue;
        int r = ws_send_text(s_ws_fds[i], text, len);
        if (r < 0) {
            /* Client gone — close and clear slot */
            close(s_ws_fds[i]);
            s_ws_fds[i] = -1;
            ESP_LOGW(TAG, "Removed dead WS client during broadcast");
        }
    }
    /* Recount after potential removals */
    int count = 0;
    for (int i = 0; i < WS_MAX_CLIENTS; i++) if (s_ws_fds[i] != -1) count++;
    xSemaphoreGive(g_ws_clients_mutex);

    g_ws_has_client = (count > 0);
}

/* ── Server task ──────────────────────────────────────────────────────────── */

static void ws_server_task(void *arg)
{
    (void)arg;
    registry_init();

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
        .sin_port        = htons(WS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind() failed: %d", errno);
        close(server_fd);
        vTaskDelete(NULL);
        return;
    }
    listen(server_fd, WS_MAX_CLIENTS);

    /* Make server_fd non-blocking for select()-based accept */
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    ESP_LOGI(TAG, "Listening on port %d", WS_PORT);

    for (;;) {
        /* Build fd_set: server + all active clients */
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(server_fd, &rfds);
        int maxfd = server_fd;

        xSemaphoreTake(g_ws_clients_mutex, portMAX_DELAY);
        for (int i = 0; i < WS_MAX_CLIENTS; i++) {
            if (s_ws_fds[i] != -1) {
                FD_SET(s_ws_fds[i], &rfds);
                if (s_ws_fds[i] > maxfd) maxfd = s_ws_fds[i];
            }
        }
        xSemaphoreGive(g_ws_clients_mutex);

        struct timeval tv = { .tv_sec = 0, .tv_usec = 20000 }; /* 20ms */
        int sel = select(maxfd + 1, &rfds, NULL, NULL, &tv);

        /* Use do-while(0) so we can break out to the broadcast drain
         * on select error, skipping accept/receive handling. */
        do {
            if (sel < 0) {
                ESP_LOGW(TAG, "select() error: %d", errno);
                vTaskDelay(pdMS_TO_TICKS(10));
                break; /* skip to broadcast drain */
            }

            /* ── Accept new connections ──────────────────────────────────── */
            if (FD_ISSET(server_fd, &rfds)) {
                int client_fd = accept(server_fd, NULL, NULL);
                if (client_fd >= 0) {
                    int count = 0;
                    xSemaphoreTake(g_ws_clients_mutex, portMAX_DELAY);
                    for (int i = 0; i < WS_MAX_CLIENTS; i++)
                        if (s_ws_fds[i] != -1) count++;
                    xSemaphoreGive(g_ws_clients_mutex);

                    if (count >= WS_MAX_CLIENTS) {
                        ESP_LOGW(TAG, "Max WS clients, rejecting fd=%d", client_fd);
                        close(client_fd);
                    } else {
                        int nd = 1;
                        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nd, sizeof(nd));
                        fcntl(client_fd, F_SETFL, 0); /* blocking for handshake */
                        if (ws_do_handshake(client_fd) == 0) {
                            ws_add_client(client_fd);
                            const char *welcome =
                                "# Umbreon WiFi Bridge \xe2\x80\x94 connected";
                            ws_send_text(client_fd, welcome, (uint16_t)strlen(welcome));
                            fcntl(client_fd, F_SETFL, O_NONBLOCK);
                        } else {
                            close(client_fd);
                        }
                    }
                }
            }

            /* ── Receive from active clients ─────────────────────────────── */
            {
                int snapshot[WS_MAX_CLIENTS];
                xSemaphoreTake(g_ws_clients_mutex, portMAX_DELAY);
                memcpy(snapshot, s_ws_fds, sizeof(snapshot));
                xSemaphoreGive(g_ws_clients_mutex);

                for (int i = 0; i < WS_MAX_CLIENTS; i++) {
                    int fd = snapshot[i];
                    if (fd == -1 || !FD_ISSET(fd, &rfds)) continue;

                    char frame_buf[TCP_CMD_SIZE];
                    uint16_t frame_len = 0;
                    fcntl(fd, F_SETFL, 0); /* blocking for frame read */
                    int ret = ws_recv_frame(fd, frame_buf, sizeof(frame_buf), &frame_len);
                    fcntl(fd, F_SETFL, O_NONBLOCK);

                    if (ret < 0) {
                        ws_remove_client(fd);
                        close(fd);
                    } else if (frame_len > 0) {
                        tcp_cmd_t cmd;
                        cmd.len = frame_len;
                        memcpy(cmd.data, frame_buf, frame_len);
                        cmd.data[frame_len] = '\0';
                        xQueueSend(g_cmd_queue, &cmd, 0);
                    }
                }
            }
        } while (0);

        /* ── Drain broadcast queue → send to all WS clients ─────────────── */
        {
            uart_line_t msg;
            while (xQueueReceive(g_line_queue_ws, &msg, 0) == pdTRUE) {
                ws_broadcast(msg.data, msg.len);
            }
        }
    }
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void ws_server_start(void)
{
    xTaskCreate(ws_server_task, "ws", STACK_WS, NULL, PRI_WS, NULL);
}
