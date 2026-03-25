#pragma once

#include <stdint.h>

/* Create and start the WebSocket server task (port 81, PRI_WS, STACK_WS).
 * Supports up to WS_MAX_CLIENTS simultaneous connections. */
void ws_server_start(void);

/* Broadcast a text message to all connected WebSocket clients.
 * Thread-safe (uses g_ws_clients_mutex). */
void ws_broadcast(const char *text, uint16_t len);
