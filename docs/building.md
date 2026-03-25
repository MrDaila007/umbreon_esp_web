# Building

## Prerequisites

### ESP8266 RTOS SDK

Clone and set up the SDK:

```bash
git clone --recursive https://github.com/espressif/ESP8266_RTOS_SDK.git ~/ESP8266_RTOS_SDK
cd ~/ESP8266_RTOS_SDK
pip install -r requirements.txt
export IDF_PATH=~/ESP8266_RTOS_SDK
```

### Xtensa Toolchain

The SDK install script handles this:

```bash
cd ~/ESP8266_RTOS_SDK
./install.sh
source export.sh
```

The toolchain is installed at `~/.espressif/tools/xtensa-lx106-elf/`.

## WiFi Configuration

Before building, create your WiFi config file:

```bash
cp include/wifi_config_example.h include/wifi_config.h
```

Edit `include/wifi_config.h` with your credentials:

```c
#define CFG_WIFI_MODE   CFG_MODE_STA

#define STA1_SSID       "YourNetwork"
#define STA1_PASS       "YourPassword"

#define STA2_SSID       "BackupNetwork"
#define STA2_PASS       "BackupPassword"

#define AP_SSID         "UmbreonCar"
#define AP_PASS         "your_ap_password"  // min 8 chars for WPA2
```

> **Note**: `wifi_config.h` is git-ignored. Never commit credentials.

## Build Commands

All commands use the `Makefile` wrapper around `idf.py`:

| Command | Action |
|---------|--------|
| `make all` | Build firmware |
| `make flash` | Flash to `/dev/ttyUSB0` |
| `make monitor` | Serial monitor @ 74880 baud |
| `make menuconfig` | Interactive SDK configuration |
| `make clean` | Full clean (removes build/) |
| `make test` | Run host-side unit tests |

### Build

```bash
make all
```

Output binary is in `build/umbreon_esp_web.bin`.

### Flash

Connect the Wemos D1 Mini via USB and run:

```bash
make flash
```

Default serial port is `/dev/ttyUSB0`. To use a different port:

```bash
idf.py -p /dev/ttyACM0 flash
```

### Monitor

```bash
make monitor
```

Exit with `Ctrl+]`.

### Combined Flash + Monitor

```bash
idf.py flash monitor
```

## SDK Configuration

Key settings in `sdkconfig.defaults`:

| Setting | Value | Reason |
|---------|-------|--------|
| CPU frequency | 160 MHz | Maximum for ESP8266 |
| lwIP max sockets | 10 | 3 server + 4 WS + 1 TCP + headroom |
| mbedTLS SHA-1 | Enabled | Required for WebSocket handshake |
| Log level | INFO | Change to WARN for production |
| Flash size | 2 MB | Wemos D1 Mini default |
| Flash mode | DIO | Compatible with most boards |
| Partition | Single-app | No OTA |
| UART console | UART0 (GPIO1/3) | `ESP_LOG` output |

To customize:

```bash
make menuconfig
```

## Build System Internals

The project uses the ESP-IDF CMake build system:

- **Top-level** `CMakeLists.txt`: Includes IDF project macro
- **main/** `CMakeLists.txt`: Registers the application component
  - Sources: 8 `.c` files + 1 `.cpp` file
  - Requires: `freertos`, `lwip`, `mbedtls`, `esp8266`, `nvs_flash`, `log`, `tcpip_adapter`, `pthread`
  - Include dirs: `main/` and `include/`

## Troubleshooting

### "wifi_config.h not found"

Copy the example: `cp include/wifi_config_example.h include/wifi_config.h`

### Flash fails with permission error

Add your user to the dialout group:

```bash
sudo usermod -aG dialout $USER
# Log out and back in
```

### Build fails with IDF_PATH

Make sure the SDK environment is sourced:

```bash
export IDF_PATH=~/ESP8266_RTOS_SDK
source $IDF_PATH/export.sh
```
