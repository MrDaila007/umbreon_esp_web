#include "http_server.h"
#include "app_config.h"
#include "web_ui.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>

static const char *TAG = "http";

/* ── Helpers ──────────────────────────────────────────────────────────────── */

/* Read and discard HTTP request headers until blank line or timeout (3s). */
static void drain_request(int fd)
{
    struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char buf[256];
    int pos = 0;
    while (1) {
        int n = recv(fd, buf + pos, sizeof(buf) - 1 - pos, 0);
        if (n <= 0) break;
        pos += n;
        if (pos >= 4) {
            buf[pos] = '\0';
            if (strstr(buf, "\r\n\r\n") || strstr(buf, "\n\n")) break;
        }
        if (pos >= (int)sizeof(buf) - 1) break; /* buffer full — stop */
    }

    tv.tv_sec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

/* ── Server task ──────────────────────────────────────────────────────────── */

static void http_server_task(void *arg)
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
        .sin_port        = htons(HTTP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind() failed: %d", errno);
        close(server_fd);
        vTaskDelete(NULL);
        return;
    }
    listen(server_fd, 2); /* backlog 2: second browser tab queued */
    ESP_LOGI(TAG, "Listening on port %d", HTTP_PORT);

    size_t page_len = strlen(PAGE_HTML);

    for (;;) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            ESP_LOGW(TAG, "accept() error: %d", errno);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        int nd = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nd, sizeof(nd));

        drain_request(client_fd);

        /* Send HTTP response header */
        char hdr[128];
        int hdr_len = snprintf(hdr, sizeof(hdr),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %u\r\n"
            "Connection: close\r\n"
            "\r\n",
            (unsigned)page_len);
        send(client_fd, hdr, hdr_len, 0);

        /* Stream PAGE_HTML in chunks to avoid blocking for 25KB at once */
        const char *p = PAGE_HTML;
        size_t remaining = page_len;
        while (remaining > 0) {
            size_t chunk = remaining < HTTP_CHUNK_SIZE ? remaining : HTTP_CHUNK_SIZE;
            int sent = send(client_fd, p, chunk, 0);
            if (sent <= 0) break;
            p += sent;
            remaining -= sent;
        }

        close(client_fd);
    }
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void http_server_start(void)
{
    xTaskCreate(http_server_task, "http", STACK_HTTP, NULL, PRI_HTTP, NULL);
}
