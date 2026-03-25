#pragma once

#include <stdint.h>

/* Perform RFC 6455 WebSocket upgrade on an already-accepted TCP socket.
 *
 * Reads HTTP request headers, extracts Sec-WebSocket-Key, computes the
 * accept hash (SHA-1 + Base64 via mbedTLS), and sends the 101 response.
 *
 * Returns 0 on success, -1 on error (malformed request, wrong headers,
 * timeout, or send failure).  The caller must close(fd) on failure.
 */
int ws_do_handshake(int fd);

/* Send a WebSocket text frame (opcode 0x1, FIN=1, no masking).
 * Returns number of bytes sent, or -1 on error. */
int ws_send_text(int fd, const char *text, uint16_t len);

/* Receive one WebSocket frame.
 * On text frame:  copies unmasked payload to out_buf, sets *out_len, returns 0.
 * On close frame: sends close response, returns -1.
 * On error:       returns -1.
 * Frames larger than out_size are truncated and a CLOSE 1009 is sent. */
int ws_recv_frame(int fd, char *out_buf, uint16_t out_size, uint16_t *out_len);
