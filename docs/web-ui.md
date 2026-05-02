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

Expanded by default (click the **Settings** header to collapse). After **Read**
(`$GET`), fields are grouped (obstacles, speed, PID, steering, loop, encoder,
**battery**, flags, sensor meta).

Notable keys:

- **SPD1 / SPD2 / SLW** — cruise speeds (m/s) and setpoint slew (m/s per second; `0` = instant)
- **KOP / KOM** — start kick (% of ESC span / ms; `KOP=0` off)
- **MSP / XSP / BSP** — ESC µs limits
- **KP / KI / KD** — speed PID
- **⚡ Battery** — **BEN** (monitor on/off), **BML**, **BLV**

Workflow: **Read** → edit → **Write** (`$SET:...`) → **Save EE** (`$SAVE`) on the Pico.

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
| Tacho | `$TEST:taho` | Tachometer test |
| ESC | `$TEST:esc` | Motor controller test |
| Speed | `$TEST:speed` | PID speed hold test |
| Autotune | `$TEST:autotune` | PID autotune (wheels up) |
| PID Tune | `$TEST:pidtune` | On-track step ID + IMC/PI suggestions |
| Reactive | `$TEST:reactive` | Reactive steering test |
| Calibrate | `$TEST:cal` | ESC calibration |

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

## UI Manifest

The controller (Pico/Zephyr) can declare which sections and fields the web
client should show by emitting a `$UI:` manifest line at boot. The ESP bridge
forwards it verbatim; all show/hide logic runs in the browser.

### Protocol

```
$UI:sec=<csv>;fld=<csv>
```

- **`sec`** — comma-separated section IDs to make visible. Anything not listed
  is hidden. Empty (`sec=`) means show all sections.
- **`fld`** — comma-separated field IDs to make visible. Covers sensor tiles,
  telemetry items, and header chips. Empty (`fld=`) means show all fields.

Send `$UICAP` from the web client (or any TCP client) to ask the controller to
re-send its manifest. The web client sends `$UICAP` automatically on each
WebSocket connect, so late-joining tabs always converge.

**Backward compatibility**: if no `$UI:` message ever arrives, the UI shows
every section and field (current behavior).

### Section IDs

| ID         | UI element                                         |
|------------|----------------------------------------------------|
| `sensors`  | Sensor distance grid + Speed / Target / Steer row  |
| `imu`      | Yaw / Heading row (inside Sensors section)         |
| `run`      | Run sub-state strip (visible only while running)   |
| `map`      | Track Map canvas + zoom/pan controls               |
| `track`    | Track Record / Save / Load / Race buttons (in Map) |
| `ctrl`     | START / STOP / MONITOR / PING / STATUS bar         |
| `settings` | Settings panel (Read / Write / Save EE)            |
| `tests`    | Hardware Tests panel                               |
| `drive`    | Manual Drive panel                                 |
| `servocal` | Servo Calibration wizard                           |
| `escmsp`   | ESC Min Speed slider                               |
| `console`  | Debug Console                                      |

### Field IDs

| ID        | Element                                   |
|-----------|-------------------------------------------|
| `s0`–`s5` | Individual sensor distance tiles          |
| `speed`   | Current speed value in Sensors row        |
| `target`  | Target speed value in Sensors row         |
| `steer`   | Steering value in Sensors row             |
| `yaw`     | Yaw rate value in IMU row                 |
| `heading` | Heading value in IMU row                  |
| `battery` | Battery voltage chip in header            |
| `rssi`    | RSSI signal chip in header                |
| Any config key (`KP`, `MSP`, …) | Corresponding settings row |

### Example

A 4-sensor build without IMU, battery monitor, or track learning:

```
$UI:sec=sensors,run,ctrl,settings,tests,drive,console;fld=s0,s1,s2,s3,steer,speed,target
```

## Design

- Dark theme with Tailwind-inspired utility classes
- Responsive grid layout
- Collapsible sections
- Toast notifications for events
- No external dependencies (fully self-contained HTML/CSS/JS)
