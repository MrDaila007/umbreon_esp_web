#include "unity/unity.h"
#include <string.h>
#include <stdint.h>

/*
 * WS frame header construction tests.
 * We test the header byte layout without using send() by reimplementing
 * the header logic as a pure function.
 */

/* Build a WebSocket text frame header into `hdr` (up to 4 bytes).
 * Returns the header length.
 * This mirrors the logic in ws_send_text() from ws_handshake.c. */
static int ws_build_text_header(uint8_t *hdr, uint16_t payload_len)
{
    hdr[0] = 0x81; /* FIN=1, opcode=TEXT */
    if (payload_len <= 125) {
        hdr[1] = (uint8_t)payload_len;
        return 2;
    } else {
        hdr[1] = 126;
        hdr[2] = (uint8_t)(payload_len >> 8);
        hdr[3] = (uint8_t)(payload_len & 0xFF);
        return 4;
    }
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

void test_ws_frame_empty_payload(void)
{
    uint8_t hdr[4];
    int len = ws_build_text_header(hdr, 0);
    TEST_ASSERT_EQUAL_INT(2, len);
    TEST_ASSERT_EQUAL_HEX8(0x81, hdr[0]); /* FIN + TEXT */
    TEST_ASSERT_EQUAL_HEX8(0x00, hdr[1]); /* length=0 */
}

void test_ws_frame_small_payload(void)
{
    uint8_t hdr[4];
    int len = ws_build_text_header(hdr, 10);
    TEST_ASSERT_EQUAL_INT(2, len);
    TEST_ASSERT_EQUAL_HEX8(0x81, hdr[0]);
    TEST_ASSERT_EQUAL_HEX8(10, hdr[1]);
}

void test_ws_frame_max_small_payload(void)
{
    uint8_t hdr[4];
    int len = ws_build_text_header(hdr, 125);
    TEST_ASSERT_EQUAL_INT(2, len);
    TEST_ASSERT_EQUAL_HEX8(0x81, hdr[0]);
    TEST_ASSERT_EQUAL_HEX8(125, hdr[1]);
}

void test_ws_frame_boundary_126(void)
{
    /* 126 requires extended 16-bit length */
    uint8_t hdr[4];
    int len = ws_build_text_header(hdr, 126);
    TEST_ASSERT_EQUAL_INT(4, len);
    TEST_ASSERT_EQUAL_HEX8(0x81, hdr[0]);
    TEST_ASSERT_EQUAL_HEX8(126, hdr[1]); /* marker for 16-bit length */
    TEST_ASSERT_EQUAL_HEX8(0x00, hdr[2]); /* 126 >> 8 */
    TEST_ASSERT_EQUAL_HEX8(0x7E, hdr[3]); /* 126 & 0xFF */
}

void test_ws_frame_256_bytes(void)
{
    uint8_t hdr[4];
    int len = ws_build_text_header(hdr, 256);
    TEST_ASSERT_EQUAL_INT(4, len);
    TEST_ASSERT_EQUAL_HEX8(0x81, hdr[0]);
    TEST_ASSERT_EQUAL_HEX8(126, hdr[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, hdr[2]); /* 256 >> 8 */
    TEST_ASSERT_EQUAL_HEX8(0x00, hdr[3]); /* 256 & 0xFF */
}

void test_ws_frame_max_16bit(void)
{
    uint8_t hdr[4];
    int len = ws_build_text_header(hdr, 65535);
    TEST_ASSERT_EQUAL_INT(4, len);
    TEST_ASSERT_EQUAL_HEX8(0x81, hdr[0]);
    TEST_ASSERT_EQUAL_HEX8(126, hdr[1]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, hdr[2]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, hdr[3]);
}

void test_ws_frame_640_bytes(void)
{
    /* WS_FRAME_SIZE from app_config.h */
    uint8_t hdr[4];
    int len = ws_build_text_header(hdr, 640);
    TEST_ASSERT_EQUAL_INT(4, len);
    TEST_ASSERT_EQUAL_HEX8(0x81, hdr[0]);
    TEST_ASSERT_EQUAL_HEX8(126, hdr[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, hdr[2]); /* 640 >> 8 */
    TEST_ASSERT_EQUAL_HEX8(0x80, hdr[3]); /* 640 & 0xFF */
}

void test_ws_frame_fin_bit_set(void)
{
    /* Verify FIN bit is always set (0x80 in first byte) */
    uint8_t hdr[4];
    ws_build_text_header(hdr, 0);
    TEST_ASSERT_BITS(0x80, 0x80, hdr[0]); /* FIN=1 */
}

void test_ws_frame_opcode_text(void)
{
    /* Verify opcode is TEXT (0x01 in lower nibble) */
    uint8_t hdr[4];
    ws_build_text_header(hdr, 0);
    TEST_ASSERT_BITS(0x0F, 0x01, hdr[0]); /* opcode=TEXT */
}
