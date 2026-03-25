#include "wifi_manager.h"
#include "shared_state.h"
#include "app_config.h"

#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "tcpip_adapter.h"
#include "nvs_flash.h"

#include <string.h>
#include <stdio.h>

/* Include user credentials (git-ignored). Falls back to example defaults. */
/* wifi_config.h is git-ignored; fall back to example defaults if absent.
 * Both files are found via ../include in INCLUDE_DIRS. */
#if __has_include("wifi_config.h")
#  include "wifi_config.h"
#else
#  include "wifi_config_example.h"
#endif

/* Resolve mode macro — renamed to avoid clash with lwIP WIFI_STA/WIFI_AP */
#define CFG_MODE_STA 0
#define CFG_MODE_AP  1

#ifndef CFG_WIFI_MODE
#  define CFG_WIFI_MODE CFG_MODE_STA
#endif

/* ── Backward compat: old STA_SSID → STA1_SSID ────────────────────────────── */
#ifdef STA_SSID
#  ifndef STA1_SSID
#    define STA1_SSID STA_SSID
#  endif
#  ifndef STA1_PASS
#    define STA1_PASS STA_PASS
#  endif
#endif

#ifndef STA1_SSID
#  define STA1_SSID ""
#endif
#ifndef STA1_PASS
#  define STA1_PASS ""
#endif
#ifndef STA2_SSID
#  define STA2_SSID ""
#endif
#ifndef STA2_PASS
#  define STA2_PASS ""
#endif

static const char *TAG = "wifi";

static SemaphoreHandle_t s_got_ip_sem = NULL;
static TimerHandle_t     s_reconnect_timer = NULL;

/* Track which SSID we're connected to (for status report) */
static const char *s_connected_ssid = "";

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static void start_ap(void)
{
    wifi_config_t ap_cfg = {
        .ap = {
            .ssid            = AP_SSID,
            .password        = AP_PASS,
            .ssid_len        = 0,
            .authmode        = WIFI_AUTH_WPA2_PSK,
            .max_connection  = 4,
            .beacon_interval = 100,
        },
    };
    if (strlen(AP_PASS) == 0) {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    g_wifi_is_sta = false;
    s_connected_ssid = AP_SSID;
    ESP_LOGI(TAG, "AP started: SSID=%s", AP_SSID);
}

/* Try connecting to a single STA network.
 * Returns true if connected within timeout_ms. */
static bool try_sta_connect(const char *ssid, const char *pass, uint32_t timeout_ms)
{
    if (ssid[0] == '\0') return false; /* skip empty SSID */

    ESP_LOGI(TAG, "Trying STA: %s (%lums)", ssid, (unsigned long)timeout_ms);

    wifi_config_t sta_cfg = { 0 };
    strncpy((char *)sta_cfg.sta.ssid,     ssid, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char *)sta_cfg.sta.password, pass,  sizeof(sta_cfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* SYSTEM_EVENT_STA_START fires → esp_wifi_connect() called in handler */

    bool ok = (xSemaphoreTake(s_got_ip_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE);
    if (ok) {
        s_connected_ssid = ssid;
        g_wifi_is_sta = true;
        ESP_LOGI(TAG, "Connected to %s", ssid);
    } else {
        ESP_LOGW(TAG, "Timeout connecting to %s", ssid);
        esp_wifi_stop();
    }
    return ok;
}

static void reconnect_timer_cb(TimerHandle_t t)
{
    (void)t;
    ESP_LOGI(TAG, "Reconnecting to STA...");
    esp_wifi_connect();
}

/* ── WiFi event handler ───────────────────────────────────────────────────── */

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    (void)ctx;
    switch (event->event_id) {

    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        g_wifi_is_sta = true;
        ESP_LOGI(TAG, "Got IP: " IPSTR,
                 IP2STR(&event->event_info.got_ip.ip_info.ip));
        if (s_got_ip_sem) {
            xSemaphoreGive(s_got_ip_sem);
        }
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        if (g_wifi_is_sta) {
            /* Was connected, now lost — schedule reconnect */
            if (s_reconnect_timer) {
                xTimerStart(s_reconnect_timer, 0);
            }
        }
        /* During initial connection: don't reconnect here.
         * Semaphore timeout in try_sta_connect() handles fallback. */
        break;

    case SYSTEM_EVENT_AP_START:
        ESP_LOGI(TAG, "AP mode started");
        break;

    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "AP: station connected");
        break;

    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "AP: station disconnected");
        break;

    default:
        break;
    }
    return ESP_OK;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void wifi_manager_init(void)
{
    /* NVS required by WiFi driver */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

#if CFG_WIFI_MODE == CFG_MODE_STA
    /* ── STA mode: try STA1 → STA2 → AP fallback ──────────────────────── */
    s_got_ip_sem = xSemaphoreCreateBinary();

    bool connected = try_sta_connect(STA1_SSID, STA1_PASS, STA_TIMEOUT_MS);
    if (!connected) {
        connected = try_sta_connect(STA2_SSID, STA2_PASS, STA_TIMEOUT_MS);
    }

    if (!connected) {
        ESP_LOGW(TAG, "All STA networks failed — starting AP");
        start_ap();
    } else {
        /* Set up reconnect timer for future disconnections */
        s_reconnect_timer = xTimerCreate("reconn", pdMS_TO_TICKS(RECONNECT_MS),
                                          pdFALSE, NULL, reconnect_timer_cb);
    }
#else
    /* ── AP mode ─────────────────────────────────────────────────────────── */
    start_ap();
#endif
}

void wifi_manager_get_status(char *buf, size_t buflen)
{
    char ip_str[16] = "0.0.0.0";

    if (g_wifi_is_sta) {
        tcpip_adapter_ip_info_t info;
        if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &info) == ESP_OK) {
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&info.ip));
        }
        snprintf(buf, buflen,
                 "# Mode:  STA\r\n"
                 "# SSID:  %s\r\n"
                 "# IP:    %s\r\n"
                 "# Status: ready\r\n",
                 s_connected_ssid, ip_str);
    } else {
        tcpip_adapter_ip_info_t info;
        if (tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &info) == ESP_OK) {
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&info.ip));
        }
        snprintf(buf, buflen,
                 "# Mode:  AP\r\n"
                 "# SSID:  %s\r\n"
                 "# IP:    %s\r\n"
                 "# Status: ready\r\n",
                 AP_SSID, ip_str);
    }
}
