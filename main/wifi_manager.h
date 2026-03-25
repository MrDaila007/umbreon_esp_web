#pragma once

#include <stddef.h>

/* Initialise WiFi.
 * In STA mode (from wifi_config.h): tries to connect, blocks up to
 * STA_TIMEOUT_MS, then falls back to AP mode automatically.
 * In AP mode: starts AP immediately.
 * On return the network is up and g_wifi_is_sta reflects the mode. */
void wifi_manager_init(void);

/* Fill buf with a human-readable WiFi status block:
 *   "# Mode:  STA\r\n# SSID:  ...\r\n# IP:    ...\r\n# Status: ready\r\n"
 * Safe to call from any task after wifi_manager_init() returns. */
void wifi_manager_get_status(char *buf, size_t buflen);

/* Get current RSSI in dBm. Returns 0 if not in STA mode. */
int wifi_manager_get_rssi(void);
