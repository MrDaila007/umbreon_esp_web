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
| `$STS:RUN` | Car is running |
| `$STS:STOP` | Car is stopped |
| `$PONG` | Response to `$PING` |
| `$ACK` | Command accepted |
| `$NAK:<msg>` | Command rejected with reason |
| `$CFG:<params>` | Configuration values response |
| `$T:<data>` | Test output |
| `$BAT:<voltage>` | Battery voltage |
| `$RSSI:<dBm>` | WiFi signal strength |
| `$TRK:<data>` | Track data |
| `$TDONE:<info>` | Track operation complete |

### Outbound (ESP -> Pico)

#### Control Commands

| Command | Description |
|---------|-------------|
| `$START` | Start autonomous driving |
| `$STOP` | Stop car |
| `$PING` | Connectivity check |
| `$STATUS` | Request status |
| `$GET` | Request configuration |
| `$SET:<params>` | Set configuration parameters |
| `$SAVE` | Save config to flash |
| `$LOAD` | Load config from flash |
| `$RST` | Reset to defaults |

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
