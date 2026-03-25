#include "unity/unity.h"

/* UNIT_TEST is defined via -DUNIT_TEST in CMakeLists.txt */
#include "led_logic.h"

/* ── Tests ────────────────────────────────────────────────────────────────── */

void test_led_connecting_flashes(void)
{
    /* CONNECTING: rapid flash with LED_FLASH_CONN_MS=80ms period */
    /* At t=0ms -> (0/80)%2 = 0 (ON) */
    TEST_ASSERT_EQUAL_INT(0, led_compute_level(LED_STATE_CONNECTING, 0, 0));
    /* At t=80ms -> (80/80)%2 = 1 (OFF) */
    TEST_ASSERT_EQUAL_INT(1, led_compute_level(LED_STATE_CONNECTING, 80, 0));
    /* At t=160ms -> (160/80)%2 = 0 (ON) */
    TEST_ASSERT_EQUAL_INT(0, led_compute_level(LED_STATE_CONNECTING, 160, 0));
    /* At t=40ms -> (40/80)%2 = 0 (ON, integer division) */
    TEST_ASSERT_EQUAL_INT(0, led_compute_level(LED_STATE_CONNECTING, 40, 0));
}

void test_led_ready_blinks_slow(void)
{
    /* READY: slow blink with LED_BLINK_READY_MS=500ms period */
    TEST_ASSERT_EQUAL_INT(0, led_compute_level(LED_STATE_READY, 0, 0));
    TEST_ASSERT_EQUAL_INT(1, led_compute_level(LED_STATE_READY, 500, 0));
    TEST_ASSERT_EQUAL_INT(0, led_compute_level(LED_STATE_READY, 1000, 0));
}

void test_led_client_idle_blinks_fast(void)
{
    /* CLIENT_IDLE: fast blink with LED_BLINK_IDLE_MS=150ms period */
    /* last_rx_ms is old so no transition to CLIENT_DATA */
    uint32_t old_rx = 0;
    uint32_t now = 10000; /* 10s ago */
    TEST_ASSERT_EQUAL_INT((now / LED_BLINK_IDLE_MS) % 2,
                          led_compute_level(LED_STATE_CLIENT_IDLE, now, old_rx));
}

void test_led_client_data_solid_on(void)
{
    /* CLIENT_DATA: solid ON = level 0 (active low) */
    TEST_ASSERT_EQUAL_INT(0, led_compute_level(LED_STATE_CLIENT_DATA, 0, 0));
    TEST_ASSERT_EQUAL_INT(0, led_compute_level(LED_STATE_CLIENT_DATA, 12345, 0));
}

void test_led_idle_transitions_to_data_on_recent_rx(void)
{
    /* CLIENT_IDLE + recent RX (within LED_DATA_TIMEOUT_MS=500ms) -> solid ON */
    uint32_t now = 5000;
    uint32_t recent_rx = 4800; /* 200ms ago, within 500ms window */
    TEST_ASSERT_EQUAL_INT(0, led_compute_level(LED_STATE_CLIENT_IDLE, now, recent_rx));
}

void test_led_idle_stays_idle_on_old_rx(void)
{
    /* CLIENT_IDLE + old RX (> LED_DATA_TIMEOUT_MS) -> stays IDLE (blinking) */
    uint32_t now = 5000;
    uint32_t old_rx = 4000; /* 1000ms ago, outside 500ms window */
    int level = led_compute_level(LED_STATE_CLIENT_IDLE, now, old_rx);
    /* Should be blinking, not solid ON */
    int expected = (now / LED_BLINK_IDLE_MS) % 2;
    TEST_ASSERT_EQUAL_INT(expected, level);
}

void test_led_idle_boundary_exactly_at_timeout(void)
{
    /* At exactly the timeout boundary: age == LED_DATA_TIMEOUT_MS -> NOT data */
    uint32_t now = 1000;
    uint32_t rx = now - LED_DATA_TIMEOUT_MS; /* age == 500 -> >= 500 -> stays IDLE */
    int level = led_compute_level(LED_STATE_CLIENT_IDLE, now, rx);
    int expected = (now / LED_BLINK_IDLE_MS) % 2;
    TEST_ASSERT_EQUAL_INT(expected, level);
}

void test_led_idle_just_inside_timeout(void)
{
    /* Just inside timeout: age == LED_DATA_TIMEOUT_MS - 1 -> transitions to DATA */
    uint32_t now = 1000;
    uint32_t rx = now - (LED_DATA_TIMEOUT_MS - 1); /* age == 499 -> < 500 -> DATA */
    TEST_ASSERT_EQUAL_INT(0, led_compute_level(LED_STATE_CLIENT_IDLE, now, rx));
}
