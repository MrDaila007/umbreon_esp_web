#include "unity/unity.h"

/* ── test_base64.c ────────────────────────────────────────────────────────── */
extern void test_base64_empty_input(void);
extern void test_base64_one_byte(void);
extern void test_base64_two_bytes(void);
extern void test_base64_three_bytes(void);
extern void test_base64_standard_vectors(void);
extern void test_base64_binary_data(void);
extern void test_base64_overflow_returns_minus1(void);
extern void test_base64_exact_fit(void);

/* ── test_header_parser.c ─────────────────────────────────────────────────── */
extern void test_header_find_existing(void);
extern void test_header_find_websocket_key(void);
extern void test_header_case_insensitive(void);
extern void test_header_case_insensitive_mixed(void);
extern void test_header_not_found(void);
extern void test_header_host(void);
extern void test_header_version(void);
extern void test_header_small_output_buffer(void);
extern void test_header_trailing_spaces(void);
extern void test_header_empty_value(void);

/* ── test_led_logic.c ─────────────────────────────────────────────────────── */
extern void test_led_connecting_flashes(void);
extern void test_led_ready_blinks_slow(void);
extern void test_led_client_idle_blinks_fast(void);
extern void test_led_client_data_solid_on(void);
extern void test_led_idle_transitions_to_data_on_recent_rx(void);
extern void test_led_idle_stays_idle_on_old_rx(void);
extern void test_led_idle_boundary_exactly_at_timeout(void);
extern void test_led_idle_just_inside_timeout(void);

/* ── test_ws_frame.c ──────────────────────────────────────────────────────── */
extern void test_ws_frame_empty_payload(void);
extern void test_ws_frame_small_payload(void);
extern void test_ws_frame_max_small_payload(void);
extern void test_ws_frame_boundary_126(void);
extern void test_ws_frame_256_bytes(void);
extern void test_ws_frame_max_16bit(void);
extern void test_ws_frame_640_bytes(void);
extern void test_ws_frame_fin_bit_set(void);
extern void test_ws_frame_opcode_text(void);

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    /* Base64 */
    RUN_TEST(test_base64_empty_input);
    RUN_TEST(test_base64_one_byte);
    RUN_TEST(test_base64_two_bytes);
    RUN_TEST(test_base64_three_bytes);
    RUN_TEST(test_base64_standard_vectors);
    RUN_TEST(test_base64_binary_data);
    RUN_TEST(test_base64_overflow_returns_minus1);
    RUN_TEST(test_base64_exact_fit);

    /* Header parser */
    RUN_TEST(test_header_find_existing);
    RUN_TEST(test_header_find_websocket_key);
    RUN_TEST(test_header_case_insensitive);
    RUN_TEST(test_header_case_insensitive_mixed);
    RUN_TEST(test_header_not_found);
    RUN_TEST(test_header_host);
    RUN_TEST(test_header_version);
    RUN_TEST(test_header_small_output_buffer);
    RUN_TEST(test_header_trailing_spaces);
    RUN_TEST(test_header_empty_value);

    /* LED state machine */
    RUN_TEST(test_led_connecting_flashes);
    RUN_TEST(test_led_ready_blinks_slow);
    RUN_TEST(test_led_client_idle_blinks_fast);
    RUN_TEST(test_led_client_data_solid_on);
    RUN_TEST(test_led_idle_transitions_to_data_on_recent_rx);
    RUN_TEST(test_led_idle_stays_idle_on_old_rx);
    RUN_TEST(test_led_idle_boundary_exactly_at_timeout);
    RUN_TEST(test_led_idle_just_inside_timeout);

    /* WS frame header */
    RUN_TEST(test_ws_frame_empty_payload);
    RUN_TEST(test_ws_frame_small_payload);
    RUN_TEST(test_ws_frame_max_small_payload);
    RUN_TEST(test_ws_frame_boundary_126);
    RUN_TEST(test_ws_frame_256_bytes);
    RUN_TEST(test_ws_frame_max_16bit);
    RUN_TEST(test_ws_frame_640_bytes);
    RUN_TEST(test_ws_frame_fin_bit_set);
    RUN_TEST(test_ws_frame_opcode_text);

    return UNITY_END();
}
