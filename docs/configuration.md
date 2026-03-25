# Configuration

All compile-time constants are centralized in `include/app_config.h`.

## Network Ports

| Constant | Default | Description |
|----------|---------|-------------|
| `TCP_PORT` | 23 | Telnet-like TCP server |
| `HTTP_PORT` | 80 | HTTP server (web UI) |
| `WS_PORT` | 81 | WebSocket server |

## UART

| Constant | Default | Description |
|----------|---------|-------------|
| `UART_NUM` | `UART_NUM_0` | UART peripheral |
| `UART_BAUD` | 115200 | Baud rate |
| `UART_RING_SIZE` | 1024 | Internal ring buffer size (bytes) |
| `UART_LINE_SIZE` | 512 | Maximum line length (bytes) |

## Buffer Sizes

| Constant | Default | Description |
|----------|---------|-------------|
| `TCP_CMD_SIZE` | 256 | Max TCP command length |
| `WS_FRAME_SIZE` | 640 | Max WebSocket outbound frame |
| `HTTP_CHUNK_SIZE` | 1024 | HTTP response streaming chunk |

## WebSocket

| Constant | Default | Description |
|----------|---------|-------------|
| `WS_MAX_CLIENTS` | 4 | Max concurrent WebSocket clients |
| `WS_GUID` | `"258EAFA5-..."` | RFC 6455 magic GUID (do not change) |

## FreeRTOS Tasks

### Stack Sizes

| Constant | Default | Task |
|----------|---------|------|
| `STACK_UART` | 2048 | uart_task |
| `STACK_TCP` | 3072 | tcp_server_task |
| `STACK_HTTP` | 3072 | http_server_task |
| `STACK_WS` | 4096 | ws_server_task |
| `STACK_LED` | 1024 | led_task |

### Priorities

| Constant | Default | Task |
|----------|---------|------|
| `PRI_UART` | 10 | uart_task (highest) |
| `PRI_WS` | 7 | ws_server_task |
| `PRI_TCP` | 6 | tcp_server_task |
| `PRI_HTTP` | 5 | http_server_task |
| `PRI_LED` | 3 | led_task (lowest) |

### Queue Depths

| Constant | Default | Description |
|----------|---------|-------------|
| `Q_LINE_DEPTH` | 8 | Telemetry broadcast queue (TCP & WS) |
| `Q_CMD_DEPTH` | 4 | Command queue (clients -> UART) |

## LED Timing

| Constant | Default | Description |
|----------|---------|-------------|
| `LED_GPIO` | 2 | GPIO pin (active LOW) |
| `LED_FLASH_CONN_MS` | 80 | CONNECTING: rapid flash period |
| `LED_BLINK_READY_MS` | 500 | READY: slow blink period |
| `LED_BLINK_IDLE_MS` | 150 | CLIENT_IDLE: medium blink period |
| `LED_DATA_TIMEOUT_MS` | 500 | CLIENT_DATA: RX timeout for solid ON |

## WiFi

| Constant | Default | Description |
|----------|---------|-------------|
| `STA_TIMEOUT_MS` | 15000 | Per-network connection timeout |
| `RECONNECT_MS` | 5000 | Auto-reconnect backoff delay |

## WiFi Credentials (wifi_config.h)

User-editable file (git-ignored). Copy from `wifi_config_example.h`:

| Define | Description |
|--------|-------------|
| `CFG_WIFI_MODE` | `CFG_MODE_STA` or `CFG_MODE_AP` |
| `STA1_SSID` / `STA1_PASS` | Primary WiFi network |
| `STA2_SSID` / `STA2_PASS` | Backup WiFi network |
| `AP_SSID` / `AP_PASS` | Access point credentials (min 8 chars) |
| `STA_TIMEOUT_MS_OVERRIDE` | Override default STA timeout |

## SDK Configuration (sdkconfig)

Managed via `make menuconfig`. Key defaults in `sdkconfig.defaults`:

| Setting | Value |
|---------|-------|
| CPU frequency | 160 MHz |
| lwIP max sockets | 10 |
| mbedTLS SHA-1 | Enabled |
| Log level | INFO |
| Flash size | 2 MB |
| Flash mode | DIO, 40 MHz |
| Partition table | Single-app |
