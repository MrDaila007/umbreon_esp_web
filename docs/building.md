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

On Ubuntu 24.04+, if `./install.sh` fails with **`/usr/bin/env: 'python': No such file or directory`**, install a `python` shim: **`sudo apt install python-is-python3`**, or run `./install.sh` with an activated Python venv (venv’s `bin/python` satisfies scripts that call `python`).

If **`git describe --tags` / exit 128 / "No names found"** after a **shallow** clone (`--depth 1`), run **`git fetch --unshallow`** and **`git fetch --tags`**, then `./install.sh` again — or clone the SDK **without** `--depth 1`.

If **`pip install --user virtualenv` / "Can not perform a '--user' install"** while a **venv is active**: run **`pip install virtualenv`** inside that venv before `./install.sh`, or **`deactivate`** and install **`python3-virtualenv`** (`apt`) for system Python, then run `./install.sh` without the SDK venv activated.

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

## CI (self-hosted)

Workflow [`.github/workflows/build.yml`](../.github/workflows/build.yml) использует тот же self-hosted runner, что и прошивка Pico (`runs-on: [self-hosted, linux, embedded]`).

На машине раннера должны быть установлены **ESP8266 RTOS SDK** и **Xtensa toolchain** по разделу [Prerequisites](#prerequisites) выше: каталог `~/ESP8266_RTOS_SDK`, `./install.sh` из SDK, чтобы `Makefile` находил `idf.py` и `~/.espressif/tools/xtensa-lx106-elf/...`. Job `test-host` ( `make test` ) нужен только `cmake` и компилятор хоста; job `build` вызывает `make all`.

Пошаговая настройка общего раннера (Proxmox, Zephyr + ESP8266, два репозитория): в соседнем проекте **umbreon_zephyr** файл `docs/self-hosted-ci.md`, раздел **§3.6 ESP8266 RTOS SDK** и подраздел **«Один раннер на Zephyr и ESP8266»**.

В CI перед `source export.sh` задайте **`export IDF_PATH="$HOME/ESP8266_RTOS_SDK"`** и не включайте **`set -u`** в том же шаге до `source`, иначе Bash может выдать `IDF_PATH: unbound variable` (см. `.github/workflows/build.yml`).

Если **`export.sh`** падает после строки про **`pkg_resources`** / **`check_python_dependencies`**, нужен **setuptools** в том же интерпретаторе, что окажется в `PATH` после `eval "$(python3 "$IDF_PATH/tools/idf_tools.py" export)"` (не обязательно путь `find` по каталогу). Проверка:  
`eval "$(python3 ~/ESP8266_RTOS_SDK/tools/idf_tools.py export)" && python3 -m pip install -U setuptools pip && source ~/ESP8266_RTOS_SDK/export.sh`.  
В CI это повторяет шаг **Build firmware** в `.github/workflows/build.yml`.
