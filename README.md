# umbreon_esp_web

ESP8266 RTOS WiFi-to-Serial bridge for the Roborace autonomous car platform.

Bridges a Raspberry Pi Pico (UART) to network clients via TCP, HTTP, and WebSocket, providing a real-time web dashboard for monitoring telemetry and controlling the car.

## Features

- **WiFi**: STA with dual-network priority fallback, automatic AP mode
- **TCP server** (port 23): Single-client telnet-like interface
- **HTTP server** (port 80): Serves embedded single-page web dashboard (~25 KB)
- **WebSocket server** (port 81): Up to 4 concurrent clients, RFC 6455 compliant
- **UART bridge**: Bidirectional line-based protocol at 115200 baud
- **Status LED**: 4-state blink pattern reflecting WiFi/client/data activity
- **Web UI**: Real-time sensor display, track map, manual drive, PID tuning, calibration, debug console

## Hardware

- **Board**: Wemos D1 Mini (ESP8266, 160 MHz)
- **UART0**: Connected to Pico (GP4/GP5) at 115200 baud, 8N1
- **GPIO2**: Status LED (active LOW)

## Quick Start

### Prerequisites

- [ESP8266 RTOS SDK](https://github.com/espressif/ESP8266_RTOS_SDK) (v3.x)
- Xtensa LX106 toolchain
- Python 3 + pip (for `idf.py`)

### Configure WiFi

```bash
cp include/wifi_config_example.h include/wifi_config.h
# Edit wifi_config.h with your SSID/password
```

### Build & Flash

```bash
make all        # Build firmware
make flash      # Flash to /dev/ttyUSB0
make monitor    # Serial monitor @ 74880 baud
```

### Run Tests

```bash
make test       # Host-side unit tests (no hardware needed)
```

## Project Structure

```
include/            Shared headers (config, state, LED logic)
main/               Application source (UART, WiFi, TCP, HTTP, WS, LED, Web UI)
test/               Host-side unit tests (Unity framework)
docs/               Documentation
  architecture.md   System architecture and task graph
  building.md       Build system and toolchain setup
  configuration.md  All configuration options
  protocols.md      UART, TCP, HTTP, WebSocket protocol details
  web-ui.md         Web dashboard features
  testing.md        Test strategy and running tests
  hardware.md       Hardware connections and pinout
  api-reference.md  Module-by-module API reference
  archive/          Code audit report
```

## Architecture Overview

```
              UART0 (Pico)
                  |
            [ uart_task ]  (pri 10)
             /          \
   g_line_queue_tcp   g_line_queue_ws
            |              |
   [ tcp_server ]    [ ws_server ]   [ http_server ]   [ led_task ]
     port 23           port 81          port 80          GPIO2
     1 client        4 clients        stateless         status LED
            \              /
             g_cmd_queue
                  |
            [ uart_task ] --> UART TX
```

## Documentation

Full documentation is in the [docs/](docs/) directory. Start with [architecture.md](docs/architecture.md) for system design, or [building.md](docs/building.md) for setup instructions.

## License

Private project.
