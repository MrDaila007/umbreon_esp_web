#include "ws_handshake.h"
#include "app_config.h"

#include "mbedtls/sha1.h"
#include "esp_log.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <errno.h>

static const char *TAG = "ws_hs";

/* ── Base64 encoder ───────────────────────────────────────────────────────── */

static const char B64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Encode `in_len` bytes from `in` into `out` (null-terminated).
 * out_size must be >= ceil(in_len/3)*4 + 1.
 * Returns number of characters written (excluding null), or -1 on overflow. */
static int base64_encode(const uint8_t *in, size_t in_len,
                         char *out, size_t out_size)
{
    size_t out_len = ((in_len + 2) / 3) * 4;
    if (out_len + 1 > out_size) return -1;

    size_t i = 0, o = 0;
    while (i < in_len) {
        /* Load up to 3 bytes, pad with zero if fewer remain */
        uint32_t b = (uint32_t)in[i] << 16;
        if (i + 1 < in_len) b |= (uint32_t)in[i + 1] << 8;
        if (i + 2 < in_len) b |= (uint32_t)in[i + 2];

        out[o++] = B64_TABLE[(b >> 18) & 0x3F];
        out[o++] = B64_TABLE[(b >> 12) & 0x3F];
        /* 3rd output char: valid if at least 2 input bytes in this group */
        out[o++] = (i + 1 < in_len) ? B64_TABLE[(b >> 6) & 0x3F] : '=';
        /* 4th output char: valid only if all 3 input bytes present */
        out[o++] = (i + 2 < in_len) ? B64_TABLE[b & 0x3F] : '=';

        i += 3;
    }
    out[o] = '\0';
    return (int)o;
}

/* ── recv helpers ─────────────────────────────────────────────────────────── */

/* Read exactly `n` bytes.  Returns n on success, -1 on error/close. */
static int recv_exact(int fd, uint8_t *buf, int n)
{
    int received = 0;
    while (received < n) {
        int r = recv(fd, buf + received, n - received, 0);
        if (r <= 0) return -1;
        received += r;
    }
    return received;
}

/* Read HTTP headers (until \r\n\r\n) with a total timeout.
 * buf is null-terminated on success.
 * Returns length on success, -1 on timeout/overflow/error. */
static int recv_http_headers(int fd, char *buf, int bufsize)
{
    /* Use SO_RCVTIMEO for the handshake timeout */
    struct timeval tv = {
        .tv_sec  = WS_HS_TIMEOUT_MS / 1000,
        .tv_usec = (WS_HS_TIMEOUT_MS % 1000) * 1000,
    };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int pos = 0;
    while (pos < bufsize - 1) {
        int n = recv(fd, buf + pos, bufsize - 1 - pos, 0);
        if (n <= 0) {
            ESP_LOGW(TAG, "Header recv error: %d (errno %d)", n, errno);
            return -1;
        }
        pos += n;
        buf[pos] = '\0';
        if (strstr(buf, "\r\n\r\n")) {
            break;
        }
    }

    /* Remove timeout */
    tv.tv_sec = 0; tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    return pos;
}

/* Case-insensitive header value extractor.
 * Finds "Header-Name: value\r\n" and copies value into out (null-terminated).
 * Returns 0 on success, -1 if not found. */
static int get_header_value(const char *headers, const char *name,
                            char *out, size_t out_size)
{
    /* Build search pattern "name:" */
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s:", name);

    /* Case-insensitive search */
    const char *p = headers;
    while (*p) {
        /* Try matching pattern at current position */
        const char *h = p;
        const char *n = pattern;
        bool match = true;
        while (*n) {
            if ((*h | 0x20) != (*n | 0x20)) { match = false; break; }
            h++; n++;
        }
        if (match) {
            /* Skip optional spaces */
            while (*h == ' ') h++;
            /* Copy until \r or \n */
            size_t i = 0;
            while (*h && *h != '\r' && *h != '\n' && i < out_size - 1) {
                out[i++] = *h++;
            }
            out[i] = '\0';
            /* Trim trailing spaces */
            while (i > 0 && out[i-1] == ' ') out[--i] = '\0';
            return 0;
        }
        /* Advance to next line */
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
    return -1;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

int ws_do_handshake(int fd)
{
    char headers[512];
    if (recv_http_headers(fd, headers, sizeof(headers)) < 0) {
        ESP_LOGW(TAG, "Failed to read HTTP headers");
        return -1;
    }

    /* Verify Upgrade: websocket */
    char upgrade[32];
    if (get_header_value(headers, "Upgrade", upgrade, sizeof(upgrade)) < 0 ||
        strncasecmp(upgrade, "websocket", 9) != 0) {
        ESP_LOGW(TAG, "Missing or bad Upgrade header");
        return -1;
    }

    /* Extract Sec-WebSocket-Key (24 base64 chars) */
    char ws_key[32];
    if (get_header_value(headers, "Sec-WebSocket-Key", ws_key, sizeof(ws_key)) < 0 ||
        strlen(ws_key) < 20) {
        ESP_LOGW(TAG, "Missing Sec-WebSocket-Key");
        return -1;
    }

    /* Concatenate key + GUID (60 chars total) */
    char concat[64];
    int concat_len = snprintf(concat, sizeof(concat), "%s%s", ws_key, WS_GUID);
    if (concat_len != 60) {
        ESP_LOGW(TAG, "Unexpected concat length: %d", concat_len);
        return -1;
    }

    /* SHA-1 */
    uint8_t sha1_buf[20];
    mbedtls_sha1((unsigned char *)concat, (size_t)concat_len, sha1_buf);

    /* Base64 encode */
    char accept_key[32];
    if (base64_encode(sha1_buf, 20, accept_key, sizeof(accept_key)) < 0) {
        return -1;
    }

    /* Send 101 Switching Protocols */
    char response[256];
    int resp_len = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key);

    int sent = send(fd, response, resp_len, 0);
    if (sent != resp_len) {
        ESP_LOGW(TAG, "Failed to send 101 response");
        return -1;
    }

    ESP_LOGI(TAG, "WebSocket upgrade OK, fd=%d", fd);
    return 0;
}

int ws_send_text(int fd, const char *text, uint16_t len)
{
    uint8_t hdr[4];
    int hdr_len;

    hdr[0] = 0x81; /* FIN=1, opcode=TEXT */

    if (len <= 125) {
        hdr[1] = (uint8_t)len;
        hdr_len = 2;
    } else {
        hdr[1] = 126;
        hdr[2] = (uint8_t)(len >> 8);
        hdr[3] = (uint8_t)(len & 0xFF);
        hdr_len = 4;
    }

    /* Send header and payload together to avoid fragmentation.
     * MSG_MORE is not available in all lwIP builds; combine into one call. */
    if (len <= 125) {
        /* Small frame: single send with header prepended */
        uint8_t frame[2 + 125];
        frame[0] = hdr[0];
        frame[1] = hdr[1];
        memcpy(frame + 2, text, len);
        return send(fd, frame, 2 + len, 0);
    } else {
        /* Larger frame: send header then payload */
        int r = send(fd, hdr, hdr_len, 0);
        if (r < 0) return -1;
        return send(fd, text, len, 0);
    }
}

int ws_recv_frame(int fd, char *out_buf, uint16_t out_size, uint16_t *out_len)
{
    *out_len = 0;

    /* Read first 2 header bytes */
    uint8_t hdr[2];
    if (recv_exact(fd, hdr, 2) < 0) return -1;

    /* uint8_t fin    = (hdr[0] >> 7) & 1; */
    uint8_t opcode  = hdr[0] & 0x0F;
    uint8_t masked  = (hdr[1] >> 7) & 1;
    uint64_t payload_len = hdr[1] & 0x7F;

    /* Extended payload length */
    if (payload_len == 126) {
        uint8_t ext[2];
        if (recv_exact(fd, ext, 2) < 0) return -1;
        payload_len = ((uint64_t)ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        if (recv_exact(fd, ext, 8) < 0) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++) payload_len = (payload_len << 8) | ext[i];
    }

    /* Read masking key (client→server frames MUST be masked per RFC 6455) */
    uint8_t mask[4] = {0, 0, 0, 0};
    if (masked) {
        if (recv_exact(fd, mask, 4) < 0) return -1;
    }

    /* Handle close frame */
    if (opcode == 0x08) {
        /* Send close response */
        uint8_t close_frame[2] = {0x88, 0x00};
        send(fd, close_frame, 2, 0);
        return -1;
    }

    /* Only handle text frames (0x1) and continuation (0x0) */
    if (opcode != 0x01 && opcode != 0x00) {
        /* Skip unknown frames by draining payload */
        uint8_t sink[32];
        uint64_t rem = payload_len;
        while (rem > 0) {
            uint64_t chunk = rem < sizeof(sink) ? rem : sizeof(sink);
            if (recv_exact(fd, sink, (int)chunk) < 0) return -1;
            rem -= chunk;
        }
        return 0; /* non-fatal, caller will try again */
    }

    /* Reject oversized frames */
    if (payload_len >= out_size) {
        ESP_LOGW(TAG, "WS frame too large: %llu", payload_len);
        /* Drain payload */
        uint8_t sink[32];
        uint64_t rem = payload_len;
        while (rem > 0) {
            uint64_t chunk = rem < sizeof(sink) ? rem : sizeof(sink);
            if (recv_exact(fd, sink, (int)chunk) < 0) return -1;
            rem -= chunk;
        }
        /* Send CLOSE 1009 Message Too Big */
        uint8_t close_frame[4] = {0x88, 0x02, 0x03, 0xF1};
        send(fd, close_frame, 4, 0);
        return -1;
    }

    /* Read and unmask payload */
    uint16_t plen = (uint16_t)payload_len;
    if (recv_exact(fd, (uint8_t *)out_buf, plen) < 0) return -1;

    if (masked) {
        for (uint16_t i = 0; i < plen; i++) {
            out_buf[i] ^= mask[i % 4];
        }
    }
    out_buf[plen] = '\0';
    *out_len = plen;
    return 0;
}
