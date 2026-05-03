# Protocols

## UART Protocol

Line-based text protocol over UART0 at 115200 baud, 8N1. Lines are delimited by `\r\n` (CR/LF).

### Inbound (Pico -> ESP)

#### Telemetry Lines

Format: `ms,s0,s1,s2,s3[,s4,s5],steer,speed,target[,yaw,heading]`

Fields are comma-separated integers. The number of sensor fields varies (4 or 6 distance sensors).

#### Status Messages

| Message | Description |
|---------|-------------|
| `# Mode: STA\|AP` | WiFi mode header |
| `# SSID: <name>` | Connected network name |
| `# IP: <addr>` | Assigned IP address |
| `# RSSI: <dBm>` | Signal strength (STA only) |

#### Responses

| Message | Description |
|---------|-------------|
| `$STS:RUN` | Car is running (autonomous) |
| `$STS:STOP` | Car is stopped |
| `$STS:MONITOR` | Monitor mode (sensors + servo, no motor) |
| `$STS:STARTING` | Countdown active (5 s before autonomous) |
| `$PONG` | Response to `$PING` |
| `$ACK` | Command accepted |
| `$NAK:<msg>` | Command rejected with reason |
| `$CFG:<params>` | Configuration values response |
| `$T:<data>` | Test output |
| `$TR:<data>` | Test result data |
| `$TDONE:<name>` | Test complete |
| `$BAT:<voltage>` | Battery voltage |
| `$RSSI:<dBm>` | WiFi signal strength |
| `$RUN:<state>,<stuck>,<turns>,<clear>,<diff>` | Run sub-state telemetry (every 200 ms) |
| `$L:<text>` | Debug log line (when `$LOG:ON` active) |
| `$DIAG:<params>` | Diagnostics response |
| `$SNS:<params>` | Sensor details response |
| `$IMU:<params>` | IMU status response |
| `$PID:<params>` | PID controller state |
| `$SYS:<params>` | System info response |
| `$TRK:<data>` | Track data |
| `$TDONE:<info>` | Track operation complete |
| `$UI:sec=<csv>;fld=<csv>` | UI manifest — declares which sections and fields the web client should display (see [Web UI](web-ui.md#ui-manifest)) |

#### Configuration Keys (`$CFG:` / `$SET:`)

`$GET` returns `$CFG:KEY=VAL,KEY=VAL,...`. `$SET:KEY=VAL[,KEY=VAL,...]` writes one or more keys atomically.

| Key | Type | Description |
|-----|------|-------------|
| `FOD` | int | Front obstacle distance threshold (cm) |
| `SOD` | int | Side open distance threshold (cm) |
| `ACD` | int | All-close distance threshold (cm) |
| `CFD` | int | Close-front distance threshold (cm) |
| `SPD1` | float | Cruise speed — clear path (m/s) |
| `SPD2` | float | Cruise speed — blocked path (m/s) |
| `SLW` | float | Speed setpoint slew rate (m/s per loop); 0 = instant |
| `COE1` | float | Speed coefficient — clear |
| `COE2` | float | Speed coefficient — blocked |
| `KOP` | int | Start kick magnitude (% of ESC span); 0 = off |
| `KOM` | int | Start kick duration (ms) |
| `MSP` | int | ESC min forward speed (µs) |
| `XSP` | int | ESC max speed (µs) |
| `BSP` | int | ESC min reverse speed (µs) |
| `RVT` | int | Reverse time during recovery maneuver (ms) |
| `TRT` | int | Turn time during recovery maneuver (ms) |
| `RVS` | float | Reverse speed during recovery (m/s) |
| `KP` | float | Speed PID proportional gain |
| `KI` | float | Speed PID integral gain |
| `KD` | float | Speed PID derivative gain |
| `MNP` | int | Servo min angle (°) |
| `NTP` | int | Servo neutral angle (°) |
| `XNP` | int | Servo max angle (°) |
| `LMS` | int | Main loop period (ms) |
| `STK` | int | Stuck counter threshold (loops) |
| `STL` | int | Stall counter threshold (loops) |
| `WDD` | float | Wrong-direction speed threshold (m/s) |
| `ENH` | int | Encoder holes per revolution |
| `WDM` | float | Wheel diameter (m) |
| `TGF` | int | Tachometer glitch filter (µs) |
| `RCW` | bool | Race direction clockwise (1/0) |
| `IMR` | bool | IMU rotation inverted (1/0) |
| `SVR` | bool | Servo direction reversed (1/0) |
| `CAL` | bool | Servo calibrated flag (1/0) |
| `BEN` | bool | Battery monitor enabled (1/0) |
| `BML` | float | Battery voltage multiplier |
| `BLV` | float | Battery low voltage threshold (V) |
| `IMU` | bool (RO) | IMU present |
| `DBG` | bool (RO) | Debug mode |
| `SNS` | int (RO) | Sensor count (4 or 6) |
| `SMX` | int (RO) | Sensor max range (cm) |

### Outbound (ESP -> Pico)

#### Control Commands

| Command | Description |
|---------|-------------|
| `$START` | Start autonomous driving (5 s countdown) |
| `$STOP` | Stop car |
| `$MONITOR` | Monitor mode (sensors + servo, no motor) |
| `$PING` | Connectivity check |
| `$STATUS` | Request status (`$STS:RUN/STOP/MONITOR/STARTING`) |
| `$GET` | Request configuration |
| `$SET:<params>` | Set configuration parameters |
| `$SAVE` | Save config to flash |
| `$LOAD` | Load config from flash |
| `$RST` | Reset to defaults |
| `$BAT` | Request battery voltage |
| `$DIAG` | Request diagnostics |
| `$SNS` | Request sensor details |
| `$IMU` | Request IMU status |
| `$PID` | Request PID state |
| `$SYS` | Request system info |
| `$LOG:ON` | Enable debug log forwarding (`$L:` prefix) |
| `$LOG:OFF` | Disable debug log forwarding |
| `$HELP` | List available commands |
| `$UICAP` | Request the controller re-send its `$UI:` manifest |

#### Test Commands

| Command | Description |
|---------|-------------|
| `$TEST:lidar` | LiDAR sensor test |
| `$TEST:servo` | Servo test |
| `$TEST:esc` | ESC test |
| `$TEST:speed` | Speed test |
| `$TEST:autotune` | PID autotune |
| `$TEST:cal` | Calibration mode |

#### Manual Drive

| Command | Description |
|---------|-------------|
| `$DRVEN` | Enable manual drive mode |
| `$DRVOFF` | Disable manual drive mode |
| `$DRV:<steer>,<speed>` | Set steering and speed |

#### Servo / ESC Calibration

| Command | Description |
|---------|-------------|
| `$SRV:<angle>` | Move servo to angle |
| `$ESC:<microseconds>` | Set ESC pulse width |

#### Track Commands

| Command | Description |
|---------|-------------|
| `$TRK:LEARN` | Start track learning |
| `$TRK:STOP` | Stop track learning |
| `$TRK:SAVE` | Save track |
| `$TRK:LOAD` | Load track |
| `$TRK:RACE` | Start race mode |
| `$TRK:GET` | Get track data |
| `$TRK:CLEAR` | Clear track data |
| `$TRK:STATUS` | Get track status |

### Local Interception

These commands from the Pico are handled by the ESP locally (not forwarded to network clients):

| Command | Response |
|---------|----------|
| `#WIFISTATUS` | WiFi status block (`# Mode:`, `# SSID:`, `# IP:`, `# RSSI:`) |
| `$RSSI` | `$RSSI:<dBm>` value |

## TCP Server (Port 23)

Telnet-like plaintext protocol. One client at a time.

### Connection

- New connections displace existing ones (the old client is disconnected)
- On connect, server sends a banner with WiFi mode, SSID, and IP
- 10ms select timeout for non-blocking operation

### Data Flow

- **Server -> Client**: Telemetry lines from UART (one per line)
- **Client -> Server**: Command strings, queued to `g_cmd_queue` for UART TX

## HTTP Server (Port 80)

Stateless HTTP/1.0 server.

### Endpoints

| Method | Path | Response |
|--------|------|----------|
| GET | `/*` | 200 OK + `PAGE_HTML` (~25 KB, streamed in 1 KB chunks) |
| Other | `/*` | 405 Method Not Allowed |

- 3-second receive timeout
- Request headers are drained but not parsed (only method is checked)

## WebSocket Server (Port 81)

RFC 6455 compliant WebSocket server.

### Handshake

Standard HTTP upgrade:

1. Client sends `GET / HTTP/1.1` with `Upgrade: websocket` and `Sec-WebSocket-Key: <base64>`
2. Server computes `SHA-1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")` -> Base64
3. Server responds with `101 Switching Protocols` and `Sec-WebSocket-Accept: <hash>`

### Frames

- **Text frames** (opcode 0x01) only, FIN=1
- **TX** (server -> client): No masking (per RFC)
- **RX** (client -> server): Masked payloads (unmasked on receive)
- **Payload length encoding**: <= 125 bytes (1 byte), 126-65535 (2 bytes extended)
- **Close frame** (opcode 0x08): Server sends close response, disconnects client
- **Max payload**: 640 bytes (configurable via `WS_FRAME_SIZE`)
- Oversized frames: Server sends close with status 1009 (message too big)

### Client Management

- Up to 4 concurrent clients (`WS_MAX_CLIENTS`)
- Client FDs stored in array, protected by `g_ws_clients_mutex`
- 20ms select timeout for polling
- Welcome message sent after successful handshake
- Telemetry broadcast to all connected clients

### Data Flow

- **Server -> Client**: UART telemetry lines as text frames
- **Client -> Server**: Command strings, queued to `g_cmd_queue`
