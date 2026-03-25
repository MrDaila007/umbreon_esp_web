# Code Audit Report: umbreon_esp_web

**Date:** 2026-03-25
**Scope:** Full source code review of the ESP8266 RTOS WiFi-to-Serial bridge
**Codebase:** 9 source files (~1200 LOC), ESP8266 RTOS SDK v3.x, FreeRTOS

---

## 1. Architecture Overview

WiFi bridge between a Raspberry Pi Pico (UART) and network clients (TCP, HTTP, WebSocket). All telemetry from the car is received on UART0 and broadcast to connected clients; commands from clients are forwarded to UART.

### Task Map

| Task | File | Port / HW | Priority | Stack | Purpose |
|------|------|-----------|----------|-------|---------|
| uart_task | `uart_driver.c` | UART0 @ 115200 | 10 | 2 KB | RX lines from car, TX commands, intercepts `#WIFISTATUS` / `$RSSI` |
| ws_server_task | `ws_server.c` | TCP:81 | 7 | 4 KB | Up to 4 WebSocket clients, RFC 6455 |
| tcp_server_task | `tcp_server.c` | TCP:23 | 6 | 3 KB | Single telnet client |
| http_server_task | `http_server.c` | TCP:80 | 5 | 3 KB | Serves embedded web UI (~25 KB HTML/CSS/JS) |
| led_task | `led_task.c` | GPIO2 | 3 | 1 KB | Status LED state machine (4 blink patterns) |
| wifi (event-driven) | `wifi_manager.c` | WiFi radio | -- | -- | STA1 -> STA2 -> AP fallback |

### Inter-Task Communication

```
                       g_line_queue_tcp (8 items)
  uart_task ──────────────────────────────────────> tcp_server_task
       │
       │             g_line_queue_ws  (8 items)
       └──────────────────────────────────────────> ws_server_task
                                                          │
              g_cmd_queue (4 items)                       │
  uart_task <─────────────────────────────── tcp_server / ws_server

  Volatile flags (g_led_state, g_*_has_client) ──> led_task
```

---

## 2. Security Findings

### S-1 [CRITICAL] Real WiFi credentials in source tree

**File:** `include/wifi_config.h:9-10, 21-22`

The file contains real SSID/password pairs. Although `.gitignore` excludes `wifi_config.h`, if it was ever committed the credentials are in git history.

**Recommendation:** Verify git history is clean (`git log --all -- include/wifi_config.h`). If found, use `git filter-repo` to scrub. Consider a build-time credential injection instead (e.g., `idf.py menuconfig` KConfig entries).

### S-2 [MEDIUM] No authentication on network services

All three servers (TCP:23, HTTP:80, WS:81) accept connections without any authentication. Any device on the same WiFi network can send arbitrary commands to the car.

**Impact:** Acceptable for isolated competition/lab WiFi. Risky on shared networks.

**Recommendation:** For shared-network use, add a simple shared-secret query parameter for WS (`ws://host:81/?key=XXX`) and a `Authorization` header check for TCP.

### S-3 [MEDIUM] HTTP server serves 200 OK to any request

**File:** `http_server.c:20-40`

`drain_request()` discards all HTTP headers without parsing the request method or path. Any input (including malformed) gets a 200 OK with the full page. Not exploitable but could confuse vulnerability scanners.

**Recommendation:** Low priority. If desired, check for `GET` method before responding.

### S-4 [LOW] Weak default AP password

**File:** `app_config.h:61` — `AP_PASS "12345678"`

Meets WPA2 minimum (8 chars) but is trivially guessable. Only used when STA connection fails.

**Recommendation:** Acceptable for fallback mode. Document that users should change it in `wifi_config.h`.

---

## 3. Concurrency Findings

> **Context:** ESP8266 is single-core. Volatile reads/writes of aligned 32-bit values are effectively atomic. These findings are benign on ESP8266 but would be real data races on multi-core targets (ESP32, RP2350).

### C-1 [MEDIUM] `g_led_state` written by multiple tasks without mutex

**File:** `shared_state.h:45`

Written by:
- `tcp_server.c:24-26` (in `update_led()`)
- `ws_server.c:43` (in `ws_add_client()`)
- `ws_server.c:59` (in `ws_remove_client()`)
- `main.c:55,73` (during init)

Read by:
- `led_task.c:30`

On ESP8266 single-core, the `volatile` qualifier is sufficient. On multi-core, a mutex or atomic would be needed.

### C-2 [MEDIUM] `g_tcp_has_client` cross-task access

**File:** `shared_state.h:49`

Written by `tcp_server.c:22`, read by `ws_server.c:59` (to decide LED state on WS client removal). Same single-core caveat as C-1.

### C-3 [LOW] `g_last_uart_rx_ms` — single writer, single reader

**File:** `shared_state.h:46`

Written only by `uart_driver.c:45`, read only by `led_task.c:34`. Volatile is sufficient.

### C-4 [LOW] `s_connected_ssid` pointer assignment

**File:** `wifi_manager.c:63`

Written once during `wifi_manager_init()` (before servers start), then only read. Safe by initialization ordering.

---

## 4. Memory Safety Findings

### M-1 [LOW] Case-insensitive compare via `|0x20`

**File:** `ws_handshake.c:114`

```c
if ((*h | 0x20) != (*n | 0x20)) { match = false; break; }
```

Works correctly for ASCII letters (which is all HTTP header names use). Would misbehave for characters like `@` vs `` ` `` (differ by 0x20) but this cannot occur in valid HTTP header names.

### M-2 [LOW] Implicit `uint64_t` -> `uint16_t` truncation

**File:** `ws_handshake.c:305`

```c
uint16_t plen = (uint16_t)payload_len;
```

Guarded by bounds check on line 288: `if (payload_len >= out_size)` rejects anything >= 256 bytes. The truncation is safe.

### M-3 [INFO] Buffer bounds checking is consistent

All fixed-size buffers have proper length checks before writes:
- `uart_driver.c:112` — `s_uart_pos < UART_LINE_SIZE - 1`
- `tcp_server.c:136` — `cmd_pos < TCP_CMD_SIZE - 1`
- `ws_handshake.c:76` — `pos < bufsize - 1`
- `ws_handshake.c:288` — `payload_len >= out_size` rejects oversized frames

No buffer overflows found.

---

## 5. Error Handling Findings

### E-1 [MEDIUM] Unchecked `send()` for HTTP response header

**File:** `http_server.c:96`

```c
send(client_fd, hdr, hdr_len, 0);  // return value not checked
```

If this send fails, the client receives the body without the HTTP header, causing a broken response.

**Recommendation:** Check return value; if < 0, close client and skip body streaming.

### E-2 [LOW] UART overflow discards data silently

**File:** `uart_driver.c:122-128`

On `UART_FIFO_OVF` or `UART_BUFFER_FULL`, the driver flushes input and resets `s_uart_pos` but does not notify connected clients that data was lost.

**Impact:** Telemetry gaps during overflow; clients see clean but incomplete data. Acceptable for real-time telemetry where stale data is worse than gaps.

### E-3 [LOW] HTTP drain_request continues on timeout

**File:** `http_server.c:20-40`

If `recv()` times out (3s) or returns error, `drain_request()` returns and the task proceeds to send 200 OK. This is intentional — the page is served regardless.

### E-4 [INFO] Non-blocking queue drops are by design

**File:** `uart_driver.c:37-38`

```c
xQueueSend(g_line_queue_tcp, &msg, 0);  // timeout=0, drops if full
xQueueSend(g_line_queue_ws,  &msg, 0);
```

Intentional: UART task must not block on slow network consumers. Queue depth of 8 provides adequate buffering for typical telemetry rates.

---

## 6. Code Quality Assessment

### Strengths

- **Clean architecture** — Each module is a single file with clear responsibility. Tasks communicate only via FreeRTOS queues.
- **No dynamic allocation** — All server tasks use stack-allocated fixed buffers. No heap fragmentation risk.
- **Proper RFC 6455 compliance** — WebSocket handshake, framing, masking, and close frames are correctly implemented.
- **Consistent style** — `snake_case`, `static` for file-locals, `ESP_LOG` macros, `TAG` per file.
- **Good config centralization** — All tunables in `app_config.h` with documented defaults.
- **Graceful WiFi fallback** — STA1 -> STA2 -> AP with configurable timeouts.
- **Flash-resident web UI** — `PAGE_HTML` in `web_ui.cpp` raw string literal avoids RAM usage.

### Minor Observations

- `web_ui.cpp` uses C++ raw string literals solely for multi-line string embedding — effective technique, no C++ features otherwise used.
- `ws_handshake.c` implements custom `base64_encode()` instead of using mbedTLS's Base64. The custom implementation is correct and avoids pulling in the full mbedTLS Base64 module.
- Task stack sizes appear well-calibrated (no stack overflow reports in testing).

---

## 7. Recommendations Summary

| Priority | Finding | Action | Status |
|----------|---------|--------|--------|
| 1 (Critical) | S-1: Credentials in source | Verify `.gitignore`, scrub git history if needed | **CLOSED** — `.gitignore` verified, history clean, `wifi_config_example.h` exists |
| 2 (High) | E-1: Unchecked `send()` | Add return check in `http_server.c:96` | **CLOSED** — return check added, close+skip on failure |
| 3 (Medium) | S-2: No auth | Add shared-secret for WS/TCP if used on shared networks | ACCEPTED — isolated competition WiFi |
| 4 (Medium) | C-1/C-2: Volatile globals | Add mutex if porting to multi-core; acceptable on ESP8266 | **CLOSED** — safety comment added with multi-core porting caveat |
| 5 (Low) | S-3: HTTP serves any request | Optional: check request method | **CLOSED** — GET validation + 405 response added |
| 6 (Low) | E-2: Silent overflow | Optional: send `# DATA LOSS` marker to clients | **CLOSED** — `# DATA_LOSS` marker dispatched to both queues |

---

## 8. Test Strategy

### 8.1 Current State

- **No tests exist** — no test directory, no test framework, no CI.
- **No hardware abstraction layer** — modules directly call ESP-IDF and FreeRTOS APIs.

### 8.2 Three-Tier Approach

#### Tier 1: Host-Side Unit Tests (no hardware needed)

**Framework:** Unity (C, already bundled with ESP-IDF)
**Build:** Separate CMake project in `test/`, compiled with host GCC/Clang (x86_64)

Pure logic functions that can be tested directly:

| Function | File | Test Coverage |
|----------|------|---------------|
| `base64_encode()` | `ws_handshake.c:23` | Empty, 1/2/3/N bytes, padding, overflow |
| `get_header_value()` | `ws_handshake.c:99` | Present/missing/case-insensitive/trailing spaces |
| LED state machine | `led_task.c:26-65` | All 4 states, CLIENT_IDLE->DATA transition |
| WS frame header | `ws_handshake.c:201` | len=0, 125, 126, 65535 boundary cases |

**Extraction approach:**
- Wrap `static` functions with `#ifdef UNIT_TEST` to expose them
- Extract LED state machine into a pure `led_compute_level()` function

#### Tier 2: Mock-Based Integration Tests

Mock the OS/network layer to test higher-level logic:
- **Socket mocks** — ring buffers replacing `send()`/`recv()` to test WS handshake and frame I/O
- **FreeRTOS mocks** — stubs for `xQueueSend`/`xQueueReceive`/`xSemaphoreTake`/`xSemaphoreGive`

Allows testing:
- Full WS handshake (feed HTTP upgrade, verify 101 response)
- Frame decode (text, close, oversized)
- TCP line parsing (partial/complete lines, command queue)

#### Tier 3: On-Target Smoke Tests

Python scripts connecting to the real ESP8266:
- TCP telnet connection, send command, verify echo
- HTTP GET, verify HTML content
- WebSocket connect, receive telemetry, send command
- Multi-client WS (4 clients, verify broadcast)
- RSSI query via `$RSSI` command

### 8.3 Implementation Order

1. Set up `test/` directory with Unity + host CMake
2. Tier 1: `base64_encode` tests (zero refactoring)
3. Tier 1: `get_header_value` tests (zero refactoring)
4. Tier 1: LED state machine tests (extract pure function)
5. Tier 1: WS frame header tests
6. Tier 2: Socket mocks + WS handshake tests
7. Tier 3: Python smoke tests (optional, requires hardware)

### 8.4 Test Directory Structure

```
test/
  CMakeLists.txt              # Host-side build (gcc, NOT xtensa)
  unity/                      # Unity test framework (git submodule or copy)
    unity.c
    unity.h
    unity_internals.h
  mocks/
    mock_freertos.h           # Minimal FreeRTOS type stubs
    mock_esp_log.h            # ESP_LOG* -> printf
  test_base64.c               # base64_encode() tests
  test_header_parser.c        # get_header_value() tests
  test_led_logic.c            # LED state machine tests
  test_ws_frame.c             # WS frame encoding tests
  run_all_tests.c             # Unity main runner
```

### 8.5 Expected Coverage

| Module | Tier 1 | Tier 2 | Tier 3 |
|--------|--------|--------|--------|
| ws_handshake.c | base64, headers, frame build | Full handshake, frame decode | WS connect |
| led_task.c | State machine logic | -- | Visual verify |
| uart_driver.c | -- | Line parsing, cmd dispatch | UART loop |
| tcp_server.c | -- | Line accumulation | Telnet session |
| http_server.c | -- | -- | GET request |
| wifi_manager.c | -- | -- | Connect/fallback |
| ws_server.c | -- | Client registry | Multi-client |
