# Hardware

## Board

**Wemos D1 Mini** — ESP8266 (Xtensa LX106, single-core, 160 MHz)

| Spec | Value |
|------|-------|
| CPU | Xtensa LX106 @ 160 MHz |
| RAM | ~80 KB user available |
| Flash | 2 MB (SPI, DIO mode, 40 MHz) |
| WiFi | 802.11 b/g/n, 2.4 GHz |
| USB | CH340 USB-UART (for flashing and debug) |

## Pin Usage

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| GPIO1 (TX) | UART0 TX | Output | To Pico RX |
| GPIO3 (RX) | UART0 RX | Input | From Pico TX |
| GPIO2 | Status LED | Output | Active LOW, on-board LED |

## UART Connection

```
Wemos D1 Mini              Raspberry Pi Pico
┌──────────────┐           ┌──────────────┐
│         TX ──│───────────│── GP5 (RX)   │
│         RX ──│───────────│── GP4 (TX)   │
│        GND ──│───────────│── GND        │
└──────────────┘           └──────────────┘
```

UART configuration:
- Baud rate: 115200
- Data bits: 8
- Parity: None
- Stop bits: 1
- Flow control: None

> **Note**: UART0 on the ESP8266 is also used for ESP_LOG output and USB-UART debug console. During normal operation, log output is mixed with data to the Pico. Log level should be set to WARN or higher in production to minimize noise.

## Status LED

GPIO2 drives the on-board LED (active LOW: GPIO=0 means LED ON).

| LED State | Pattern | Meaning |
|-----------|---------|---------|
| CONNECTING | 80ms rapid flash | WiFi connecting |
| READY | 500ms slow blink | WiFi connected, no clients |
| CLIENT_IDLE | 150ms medium blink | Client connected, no recent data |
| CLIENT_DATA | Solid ON | Client connected, receiving data (< 500ms since last RX) |

The LED task runs at 10ms tick resolution.

## Power

The Wemos D1 Mini is powered via USB (5V) or the 5V/3.3V pins. When connected to the Pico, it typically shares a common ground and is powered from the robot's battery via a voltage regulator.

## Network Connections

After WiFi is established, the ESP exposes three services:

| Service | Port | Protocol |
|---------|------|----------|
| TCP (telnet) | 23 | Raw TCP, 1 client |
| HTTP (web UI) | 80 | HTTP/1.0, stateless |
| WebSocket | 81 | RFC 6455, up to 4 clients |

Total socket usage: up to 6 active (lwIP configured for 10).
