# Testing

## Overview

The project uses a three-tier testing strategy. Currently, Tier 1 (host-side unit tests) is fully implemented with 35 tests.

## Running Tests

```bash
make test
```

This builds and runs the host-side test suite using GCC/Clang (not the Xtensa cross-compiler). No hardware required.

### Manual Build

```bash
cd test
mkdir -p build && cd build
cmake ..
make
./run_tests
```

## Test Framework

[Unity](https://github.com/ThrowTheSwitch/Unity) — lightweight C test framework, included in `test/unity/`.

Mock headers in `test/mocks/` provide minimal stubs for ESP-IDF APIs:

- `mock_esp_log.h`: `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE` -> no-op
- `mock_freertos.h`: `xTaskGetTickCount` -> returns a controllable value

## Test Suites

### Base64 (`test_base64.c`) — 8 tests

Tests for `base64_encode()` used in WebSocket handshake:

| Test | Description |
|------|-------------|
| Empty input | Zero-length input produces empty output |
| Single byte | 1 byte -> 4 chars with padding |
| Two bytes | 2 bytes -> 4 chars with padding |
| Three bytes | 3 bytes -> 4 chars, no padding |
| Standard vector | RFC test vector "Man" -> "TWFu" |
| Binary (SHA-1) | 20-byte SHA-1 hash encoding |
| Overflow | Returns -1 when output buffer too small |
| Exact fit | Succeeds when buffer is exactly the right size |

### Header Parser (`test_header_parser.c`) — 10 tests

Tests for `get_header_value()` used in WebSocket handshake:

| Test | Description |
|------|-------------|
| Basic lookup | Finds header value by name |
| Case insensitive | Header names are case-insensitive |
| WebSocket-Key | Extracts `Sec-WebSocket-Key` value |
| Missing header | Returns empty for non-existent headers |
| Empty value | Handles headers with empty values |
| Value trimming | Strips leading/trailing whitespace |
| First occurrence | Returns first match when duplicated |
| Partial name match | No false positives on partial matches |
| Empty request | Handles empty input string |
| No colon | Handles malformed header lines |

### LED Logic (`test_led_logic.c`) — 8 tests

Tests for `led_compute_level()` pure state machine:

| Test | Description |
|------|-------------|
| CONNECTING blink | 80ms toggle period |
| READY blink | 500ms toggle period |
| CLIENT_IDLE blink | 150ms toggle period |
| CLIENT_DATA solid | LED stays ON during data activity |
| DATA timeout | Reverts to IDLE blink after 500ms of no data |
| Initial state | Correct output at tick=0 |
| Transition edge | Exact timing at state boundaries |
| Rapid toggle | Correct behavior with fast tick increments |

### WebSocket Frame (`test_ws_frame.c`) — 9 tests

Tests for `ws_build_frame()` frame encoding:

| Test | Description |
|------|-------------|
| Empty payload | Valid frame with zero-length payload |
| Small payload | 1-125 byte payloads use 1-byte length |
| 125-byte boundary | Maximum for 1-byte length encoding |
| 126-byte extended | Uses 2-byte extended length |
| Large payload | Extended length for payloads up to 640 bytes |
| FIN bit | FIN bit set in first byte |
| Text opcode | Opcode 0x01 in first byte |
| Buffer overflow | Returns error when output buffer too small |
| Max frame | 640-byte payload (WS_FRAME_SIZE limit) |

## Test Architecture

```
test/
  CMakeLists.txt         Host-side CMake build
  run_all_tests.c        Unity test runner (calls all suites)
  test_base64.c          base64_encode() tests
  test_header_parser.c   get_header_value() tests
  test_led_logic.c       LED state machine tests
  test_ws_frame.c        WS frame encoding tests
  mocks/
    mock_esp_log.h       ESP_LOG* stubs
    mock_freertos.h      FreeRTOS stubs
  unity/
    unity.c/h            Unity framework
```

### Testability Design

- `led_logic.h` contains the pure `led_compute_level()` function with no FreeRTOS or GPIO dependencies
- `ws_handshake.c` functions exposed via `#ifdef UNIT_TEST` or directly through headers
- All tested functions are pure (no side effects, no global state)

## Future Tiers

### Tier 2: Mock-Based Integration Tests (planned)

Socket mocks replacing `send()`/`recv()` to test:
- Full WebSocket handshake flow
- Frame decode with masking
- TCP line parsing and command dispatch

### Tier 3: On-Target Smoke Tests (planned)

Python scripts connecting to real hardware:
- TCP telnet session
- HTTP GET verification
- WebSocket multi-client broadcast
- RSSI query
