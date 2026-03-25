#include "unity/unity.h"
#include <string.h>

/* Pull in get_header_value from ws_handshake.c (exposed via UNIT_TEST) */
extern int get_header_value(const char *headers, const char *name,
                            char *out, size_t out_size);

/* ── Tests ────────────────────────────────────────────────────────────────── */

static const char *SAMPLE_HEADERS =
    "GET /chat HTTP/1.1\r\n"
    "Host: server.example.com\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Origin: http://example.com\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "\r\n";

void test_header_find_existing(void)
{
    char out[64];
    int ret = get_header_value(SAMPLE_HEADERS, "Upgrade", out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("websocket", out);
}

void test_header_find_websocket_key(void)
{
    char out[64];
    int ret = get_header_value(SAMPLE_HEADERS, "Sec-WebSocket-Key", out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("dGhlIHNhbXBsZSBub25jZQ==", out);
}

void test_header_case_insensitive(void)
{
    char out[64];
    int ret = get_header_value(SAMPLE_HEADERS, "upgrade", out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("websocket", out);
}

void test_header_case_insensitive_mixed(void)
{
    char out[64];
    int ret = get_header_value(SAMPLE_HEADERS, "UPGRADE", out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("websocket", out);
}

void test_header_not_found(void)
{
    char out[64];
    int ret = get_header_value(SAMPLE_HEADERS, "X-Missing-Header", out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(-1, ret);
}

void test_header_host(void)
{
    char out[64];
    int ret = get_header_value(SAMPLE_HEADERS, "Host", out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("server.example.com", out);
}

void test_header_version(void)
{
    char out[16];
    int ret = get_header_value(SAMPLE_HEADERS, "Sec-WebSocket-Version", out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("13", out);
}

void test_header_small_output_buffer(void)
{
    char out[6]; /* only fits 5 chars + null */
    int ret = get_header_value(SAMPLE_HEADERS, "Upgrade", out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, ret);
    /* Should truncate to "webso" */
    TEST_ASSERT_EQUAL_STRING("webso", out);
}

void test_header_trailing_spaces(void)
{
    const char *headers =
        "X-Test:   value with spaces   \r\n"
        "\r\n";
    char out[64];
    int ret = get_header_value(headers, "X-Test", out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, ret);
    /* Leading spaces are skipped, trailing spaces are trimmed */
    TEST_ASSERT_EQUAL_STRING("value with spaces", out);
}

void test_header_empty_value(void)
{
    const char *headers =
        "X-Empty:\r\n"
        "\r\n";
    char out[64];
    int ret = get_header_value(headers, "X-Empty", out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("", out);
}
