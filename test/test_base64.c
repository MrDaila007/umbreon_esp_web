#include "unity/unity.h"
#include <string.h>
#include <stdint.h>

/* Pull in base64_encode from ws_handshake.c (exposed via UNIT_TEST) */
extern const char B64_TABLE[];
extern int base64_encode(const uint8_t *in, size_t in_len,
                         char *out, size_t out_size);

/* ── Tests ────────────────────────────────────────────────────────────────── */

void test_base64_empty_input(void)
{
    char out[8];
    int n = base64_encode((const uint8_t *)"", 0, out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, n);
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_base64_one_byte(void)
{
    /* 'f' -> 'Zg==' */
    char out[8];
    int n = base64_encode((const uint8_t *)"f", 1, out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(4, n);
    TEST_ASSERT_EQUAL_STRING("Zg==", out);
}

void test_base64_two_bytes(void)
{
    /* 'fo' -> 'Zm8=' */
    char out[8];
    int n = base64_encode((const uint8_t *)"fo", 2, out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(4, n);
    TEST_ASSERT_EQUAL_STRING("Zm8=", out);
}

void test_base64_three_bytes(void)
{
    /* 'foo' -> 'Zm9v' (no padding) */
    char out[8];
    int n = base64_encode((const uint8_t *)"foo", 3, out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(4, n);
    TEST_ASSERT_EQUAL_STRING("Zm9v", out);
}

void test_base64_standard_vectors(void)
{
    /* RFC 4648 test vectors */
    char out[32];

    base64_encode((const uint8_t *)"foobar", 6, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Zm9vYmFy", out);

    base64_encode((const uint8_t *)"fooba", 5, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Zm9vYmE=", out);

    base64_encode((const uint8_t *)"foob", 4, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Zm9vYg==", out);
}

void test_base64_binary_data(void)
{
    /* SHA-1 output is 20 bytes -> 28 base64 chars */
    uint8_t sha1[20] = {
        0xb3, 0x7a, 0x4f, 0x2c, 0xc0, 0x62, 0x4f, 0x16,
        0x90, 0xf6, 0x46, 0x06, 0xcf, 0x38, 0x59, 0x45,
        0xb2, 0xbe, 0xc4, 0xea,
    };
    char out[32];
    int n = base64_encode(sha1, 20, out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(28, n);
    TEST_ASSERT_EQUAL_STRING("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", out);
}

void test_base64_overflow_returns_minus1(void)
{
    char out[4]; /* too small for 4 base64 chars + null */
    int n = base64_encode((const uint8_t *)"foo", 3, out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(-1, n);
}

void test_base64_exact_fit(void)
{
    /* 3 bytes -> 4 chars + null = 5 bytes needed */
    char out[5];
    int n = base64_encode((const uint8_t *)"foo", 3, out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(4, n);
    TEST_ASSERT_EQUAL_STRING("Zm9v", out);
}
