// WiFi configuration for Umbreon WiFi bridge (example).
// Copy this file to `wifi_config.h` and edit the values.
// `wifi_config.h` is git-ignored so your real credentials stay local.

#pragma once

// ── WiFi mode ──────────────────────────────────────────────────────────────
// CFG_MODE_STA — try STA1, then STA2, then fall back to AP
// CFG_MODE_AP  — start AP immediately
//
// #define CFG_WIFI_MODE CFG_MODE_STA
// #define CFG_WIFI_MODE CFG_MODE_AP

// ── STA1 credentials (primary network, e.g. home WiFi) ────────────────────
#define STA1_SSID  "YourWiFi"
#define STA1_PASS  "YourPassword"

// ── STA2 credentials (secondary network, e.g. phone hotspot) ──────────────
// Leave empty string "" to skip STA2 and go straight to AP fallback.
#define STA2_SSID  ""
#define STA2_PASS  ""

// ── Per-network timeout (ms) ──────────────────────────────────────────────
// Each network gets this long to connect before trying the next one.
// Total worst-case wait = STA_TIMEOUT_MS × 2 (if both networks fail).
#define STA_TIMEOUT_MS_OVERRIDE  15000

// ── AP credentials (fallback / competition) ────────────────────────────────
// Used when both STA networks fail, or in CFG_MODE_AP.
#define AP_SSID   "Umbreon"
#define AP_PASS   "12345678"   // min 8 chars for WPA2
