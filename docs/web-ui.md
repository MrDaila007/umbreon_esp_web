# Web UI

The embedded single-page application (SPA) is served on port 80 and communicates with the car via WebSocket on port 81. It is stored as a C++ raw string literal in `main/web_ui.cpp` (~25 KB) and lives entirely in flash.

## Accessing the Dashboard

1. Connect to the ESP8266's WiFi network (STA or AP mode)
2. Open `http://<esp-ip>/` in a browser
3. The WebSocket connection establishes automatically

## Sections

### Sensors

Displays real-time distance sensor readings (4 or 6 sensors depending on configuration). Each sensor is color-coded by distance:

- Green: safe distance
- Yellow: caution
- Red: close obstacle

Also shows current speed, steering angle, and target values.

### Track Map

2D canvas visualization of the car's path:

- Real-time position and heading
- Zoom and pan controls
- Follow-car mode (auto-centers on car)
- Track save/load/race/learn controls

### Control

| Button | Command | Description |
|--------|---------|-------------|
| Start | `$START` | Begin autonomous driving |
| Stop | `$STOP` | Stop the car |
| Ping | `$PING` | Check connectivity (expects `$PONG`) |
| Status | `$STATUS` | Request current status |

### Settings

GUI for reading and writing PID and drive parameters:

- **KP, KI, KD**: PID controller gains
- **MSP**: Max speed
- Other configurable parameters

Workflow: Read current values (`$GET`), modify in UI, write back (`$SET:...`), save to flash (`$SAVE`).

### Calibration

Servo calibration wizard:

- Move servo to min/neutral/max positions
- Set angles via `$SRV:<angle>` commands
- Save calibrated values

### Tests

Hardware test buttons:

| Test | Command | Description |
|------|---------|-------------|
| LiDAR | `$TEST:lidar` | Distance sensor test |
| Servo | `$TEST:servo` | Steering servo test |
| Tacho | `$TEST:speed` | Speed sensor test |
| ESC | `$TEST:esc` | Motor controller test |
| Speed | `$TEST:speed` | Speed control test |
| Autotune | `$TEST:autotune` | PID autotune |
| Reactive | `$TEST:cal` | Reactive test |

### Manual Drive

Slider-based manual control with safety lock:

1. Toggle safety lock to enable
2. Use steering slider (-100 to +100)
3. Use speed slider (0 to max)
4. Commands sent as `$DRV:<steer>,<speed>`
5. `$DRVEN` on enable, `$DRVOFF` on disable

### Debug Console

Real-time log viewer:

- **Telemetry tab**: Raw sensor data lines
- **Command tab**: Sent commands
- **System tab**: WiFi events, errors, status
- Regex-based filtering
- Timestamps on each entry
- 500-line scrollback buffer
- Auto-scroll with manual scroll lock

## Status Indicators

- **WiFi status**: Mode (STA/AP), SSID, IP address
- **RSSI bars**: Signal strength indicator (STA mode only)
- **Battery voltage**: Parsed from `$BAT:` messages
- **Connection dot**: Green when WebSocket is connected, red when disconnected

## WebSocket Reconnection

The UI automatically reconnects to WebSocket every 2 seconds if the connection is lost. A visual indicator shows connection status.

## Design

- Dark theme with Tailwind-inspired utility classes
- Responsive grid layout
- Collapsible sections
- Toast notifications for events
- No external dependencies (fully self-contained HTML/CSS/JS)
