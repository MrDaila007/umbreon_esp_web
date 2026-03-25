PROJECT_NAME := umbreon_esp_web

# ── SDK path ─────────────────────────────────────────────────────────────────
IDF_PATH     ?= $(HOME)/ESP8266_RTOS_SDK
IDF_PY       := python3 $(IDF_PATH)/tools/idf.py
TOOLCHAIN    := $(HOME)/.espressif/tools/xtensa-lx106-elf/esp-2020r3-49-gd5524c1-8.4.0/xtensa-lx106-elf/bin

export IDF_PATH
export PATH := $(TOOLCHAIN):$(PATH)

# ── Serial port ───────────────────────────────────────────────────────────────
PORT ?= /dev/ttyUSB0

.PHONY: all flash monitor flash-monitor menuconfig size clean test test-clean

all:
	$(IDF_PY) build

flash:
	$(IDF_PY) -p $(PORT) flash

monitor:
	$(IDF_PY) -p $(PORT) monitor

flash-monitor:
	$(IDF_PY) -p $(PORT) flash monitor

menuconfig:
	$(IDF_PY) menuconfig

size:
	$(IDF_PY) size

clean:
	$(IDF_PY) fullclean

# ── Host-side unit tests (no hardware needed) ────────────────────────────────
test:
	@cmake -B test/build test && cmake --build test/build && test/build/run_tests

test-clean:
	@rm -rf test/build
