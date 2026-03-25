# API Reference

Module-by-module reference for all public functions and interfaces.

## main.c

Entry point. Initializes all subsystems and creates FreeRTOS tasks.

```c
void app_main(void);
```

Initialization order:
1. NVS flash init
2. TCP/IP adapter init
3. Create queues and mutexes (`shared_state.h` globals)
4. `wifi_manager_init()` — blocks until WiFi connected or AP started
5. `uart_driver_init()` — configures UART0
6. Create tasks: uart, tcp_server, http_server, ws_server, led

## uart_driver.h

UART0 driver with line-based RX and command TX.

```c
void uart_driver_init(void);
void uart_task(void *arg);
```

- `uart_driver_init()`: Configures UART0 at 115200 baud, installs driver with ring buffer, attaches VFS console.
- `uart_task()`: FreeRTOS task function. Reads UART events, accumulates lines, dispatches to broadcast queues. Drains `g_cmd_queue` every 50ms and writes commands to UART TX. Intercepts `#WIFISTATUS` and `$RSSI` locally.

## wifi_manager.h

WiFi initialization and status reporting.

```c
void wifi_manager_init(void);
int  wifi_manager_format_status(char *buf, size_t size);
int  wifi_manager_get_rssi(void);
```

- `wifi_manager_init()`: Initializes WiFi subsystem. Tries STA1 (15s timeout), then STA2, then falls back to AP mode. Blocks until connected.
- `wifi_manager_format_status()`: Writes a human-readable status block (`# Mode:`, `# SSID:`, `# IP:`, `# RSSI:`) into `buf`. Returns number of bytes written.
- `wifi_manager_get_rssi()`: Returns current RSSI in dBm (STA mode only, 0 in AP mode).

## tcp_server.h

Single-client TCP server on port 23.

```c
void tcp_server_task(void *arg);
```

- Accepts one client at a time (new connections displace old)
- Sends banner on connect (WiFi status)
- Reads commands from client -> `g_cmd_queue`
- Reads telemetry from `g_line_queue_tcp` -> sends to client
- Updates `g_tcp_has_client` and `g_led_state` on connect/disconnect

## http_server.h

Stateless HTTP server on port 80.

```c
void http_server_task(void *arg);
```

- Accepts connections, drains request headers
- Validates GET method (405 for others)
- Streams `PAGE_HTML` in 1 KB chunks
- 3-second receive timeout

## ws_server.h

Multi-client WebSocket server on port 81.

```c
void ws_server_task(void *arg);
```

- Accepts up to `WS_MAX_CLIENTS` (4) concurrent connections
- Performs RFC 6455 handshake on new connections
- Broadcasts telemetry from `g_line_queue_ws` to all clients
- Receives commands from clients -> `g_cmd_queue`
- 20ms select timeout for polling

## ws_handshake.h

WebSocket protocol implementation (handshake + frame I/O).

```c
int  ws_do_handshake(int client_fd);
int  ws_send_text(int fd, const char *msg, size_t len);
int  ws_recv_frame(int fd, char *out, size_t out_size);
int  ws_build_frame(const char *payload, size_t payload_len,
                    uint8_t *out, size_t out_size);
int  base64_encode(const uint8_t *src, size_t slen,
                   char *dst, size_t dlen);
int  get_header_value(const char *request, const char *header_name,
                      char *value, size_t value_size);
```

- `ws_do_handshake()`: Reads HTTP upgrade request, validates headers, sends 101 response. Returns 0 on success.
- `ws_send_text()`: Builds and sends a text frame. Returns 0 on success.
- `ws_recv_frame()`: Reads one frame from client, unmasks payload. Returns payload length, or -1 on error/close.
- `ws_build_frame()`: Encodes payload into a WebSocket text frame (FIN=1, opcode=0x01). Returns total frame size.
- `base64_encode()`: RFC 4648 Base64 encoding. Returns 0 on success, -1 on buffer overflow.
- `get_header_value()`: Case-insensitive HTTP header value extraction. Returns 0 if found, -1 if not.

## led_task.h

Status LED driver.

```c
void led_task(void *arg);
```

- Runs at 10ms tick interval
- Reads `g_led_state` and `g_last_uart_rx_ms`
- Calls `led_compute_level()` to determine GPIO output
- Sets GPIO2 (0 = ON, 1 = OFF)

## led_logic.h

Pure LED state machine (no hardware dependencies, testable on host).

```c
static inline int led_compute_level(led_state_t state,
                                     uint32_t now_ms,
                                     uint32_t last_rx_ms);
```

- **Input**: Current LED state, current tick (ms), last UART RX tick (ms)
- **Output**: 0 (LED ON) or 1 (LED OFF)
- **States**: `LED_CONNECTING`, `LED_READY`, `LED_CLIENT_IDLE`, `LED_CLIENT_DATA`
- CLIENT_DATA returns solid ON if `(now_ms - last_rx_ms) < LED_DATA_TIMEOUT_MS`, otherwise falls through to CLIENT_IDLE blink

## shared_state.h

Global queues, flags, and mutexes shared between tasks.

```c
extern QueueHandle_t   g_line_queue_tcp;
extern QueueHandle_t   g_line_queue_ws;
extern QueueHandle_t   g_cmd_queue;
extern SemaphoreHandle_t g_ws_clients_mutex;

extern volatile led_state_t g_led_state;
extern volatile uint32_t    g_last_uart_rx_ms;
extern volatile bool        g_tcp_has_client;
extern volatile bool        g_ws_has_client;
extern volatile bool        g_wifi_is_sta;
```

## app_config.h

All compile-time constants. See [configuration.md](configuration.md) for the full list.

## web_ui.h

```c
extern const char PAGE_HTML[];
extern const size_t PAGE_HTML_LEN;
```

The embedded web dashboard. Stored in flash (`.rodata`). Defined in `web_ui.cpp` as a C++ raw string literal.
