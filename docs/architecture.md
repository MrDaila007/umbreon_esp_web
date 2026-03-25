# Architecture

## Overview

umbreon_esp_web is a FreeRTOS application running on an ESP8266 (Wemos D1 Mini). It acts as a WiFi bridge between a Raspberry Pi Pico connected via UART and network clients over TCP, HTTP, and WebSocket.

All telemetry from the Pico is received on UART0 and broadcast to connected network clients. Commands from clients are forwarded back to the Pico via UART.

## Task Graph

| Task | File | Port / HW | Priority | Stack | Purpose |
|------|------|-----------|----------|-------|---------|
| `uart_task` | `uart_driver.c` | UART0 @ 115200 | 10 | 2 KB | RX lines from Pico, TX commands, intercepts `#WIFISTATUS` / `$RSSI` |
| `ws_server_task` | `ws_server.c` | TCP:81 | 7 | 4 KB | Up to 4 WebSocket clients, RFC 6455 |
| `tcp_server_task` | `tcp_server.c` | TCP:23 | 6 | 3 KB | Single telnet-like client |
| `http_server_task` | `http_server.c` | TCP:80 | 5 | 3 KB | Serves embedded web UI (~25 KB) |
| `led_task` | `led_task.c` | GPIO2 | 3 | 1 KB | Status LED state machine (4 patterns) |
| WiFi (event-driven) | `wifi_manager.c` | WiFi radio | -- | -- | STA1 -> STA2 -> AP fallback |

## Inter-Task Communication

```
                         g_line_queue_tcp (depth 8)
  uart_task ──────────────────────────────────────> tcp_server_task
       │
       │               g_line_queue_ws (depth 8)
       └──────────────────────────────────────────> ws_server_task
                                                          │
                g_cmd_queue (depth 4)                     │
  uart_task <─────────────────────────────── tcp_server / ws_server

  Volatile flags (g_led_state, g_*_has_client) ──> led_task
```

### Queues

| Queue | Item Size | Depth | Direction |
|-------|-----------|-------|-----------|
| `g_line_queue_tcp` | 518 bytes | 8 | UART -> TCP server |
| `g_line_queue_ws` | 518 bytes | 8 | UART -> WS server |
| `g_cmd_queue` | 260 bytes | 4 | TCP/WS servers -> UART |

All queue sends are non-blocking (timeout = 0). If a queue is full, the message is dropped. This prevents the UART task from blocking on slow network consumers.

### Shared State

| Variable | Type | Writers | Readers | Protection |
|----------|------|---------|---------|------------|
| `g_led_state` | `volatile led_state_t` | tcp_server, ws_server, main | led_task | Single-core atomic |
| `g_last_uart_rx_ms` | `volatile uint32_t` | uart_task | led_task | Single-core atomic |
| `g_tcp_has_client` | `volatile bool` | tcp_server | ws_server, led_task | Single-core atomic |
| `g_ws_has_client` | `volatile bool` | ws_server | tcp_server, led_task | Single-core atomic |
| `g_wifi_is_sta` | `volatile bool` | wifi_manager | tcp_server, ws_server | Set once at init |
| WS client FDs | `int[]` | ws_server | ws_server | `g_ws_clients_mutex` |

> **Porting note**: On multi-core targets (ESP32, RP2350), the volatile globals would need mutexes or `stdatomic`.

## Message Flow

### Telemetry (Pico -> Clients)

1. `uart_task` reads bytes from UART0, accumulates until `\n`
2. Complete line is copied into a `line_msg_t` struct
3. Struct is sent to both `g_line_queue_tcp` and `g_line_queue_ws` (non-blocking)
4. `tcp_server_task` dequeues and sends to connected TCP client
5. `ws_server_task` dequeues and broadcasts to all connected WS clients as text frames

### Commands (Clients -> Pico)

1. TCP or WS server receives a command string from a client
2. Command is queued into `g_cmd_queue`
3. `uart_task` drains `g_cmd_queue` every 50ms and writes commands to UART0 TX

### Local Interception

Two commands are handled locally by `uart_task` without forwarding to network:

- `#WIFISTATUS` (from Pico): uart_task replies with WiFi status block
- `$RSSI` (from Pico): uart_task replies with `$RSSI:<dBm>` value

## WiFi State Machine

```
          ┌─────────┐
          │  START   │
          └────┬─────┘
               │
          ┌────▼─────┐     timeout (15s)    ┌──────────┐
          │ Try STA1 │ ──────────────────── │ Try STA2 │
          └────┬─────┘                      └────┬─────┘
               │ GOT_IP                          │ GOT_IP
               │                                 │
          ┌────▼─────────────────────────────────▼───┐
          │              STA Connected               │
          └────────────────┬─────────────────────────┘
                           │ DISCONNECTED
                     ┌─────▼──────┐
                     │ Reconnect  │ (5s timer)
                     │  Timer     │──── retry STA ──┐
                     └────────────┘                 │
                                                    │
          ┌──────────┐     both fail    ┌───────────▼──┐
          │  AP Mode │ <────────────── │   STA Failed  │
          └──────────┘                  └──────────────┘
```

## Memory Budget

### Static (Stack)

| Resource | Size |
|----------|------|
| uart_task stack | 2,048 B |
| tcp_server stack | 3,072 B |
| http_server stack | 3,072 B |
| ws_server stack | 4,096 B |
| led_task stack | 1,024 B |
| **Total** | **13,312 B** |

### Dynamic (Queues + Buffers)

| Resource | Size |
|----------|------|
| g_line_queue_tcp (8 x 518) | 4,144 B |
| g_line_queue_ws (8 x 518) | 4,144 B |
| g_cmd_queue (4 x 260) | 1,040 B |
| Mutexes/semaphores | ~1,024 B |
| **Total** | **~10 KB** |

ESP8266 has ~80 KB of user RAM. Total usage is well within budget.

### Flash

| Resource | Size |
|----------|------|
| SDK + bootloader | ~1.2 MB |
| Application binary | ~250 KB |
| PAGE_HTML (web UI) | ~25 KB |
| **Total** | ~1.5 MB of 2 MB |

## Design Principles

- **No dynamic allocation**: All buffers are stack-allocated or statically sized. No heap fragmentation.
- **Non-blocking telemetry**: Queue drops preferred over blocking the UART task.
- **Graceful degradation**: WiFi falls back STA1 -> STA2 -> AP. Network failures don't affect UART.
- **Flash-resident UI**: Web page stored in `.rodata` (flash), not copied to RAM.
- **Single-file modules**: Each task is a single `.c` file with clear responsibility.
