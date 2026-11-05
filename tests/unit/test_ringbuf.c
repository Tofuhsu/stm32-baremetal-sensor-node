/* Unity tests for the SPSC ring buffer (host build). */
#include "unity.h"
#include "../../src/ringbuf.h"

static ringbuf_t rb;

void setUp(void) { ringbuf_init(&rb); }
void tearDown(void) {}

static void test_starts_empty(void)
{
    uint8_t byte = 0x55;
    TEST_ASSERT_TRUE(ringbuf_empty(&rb));
    TEST_ASSERT_EQUAL_UINT32(0, ringbuf_count(&rb));
    TEST_ASSERT_FALSE(ringbuf_get(&rb, &byte));
    TEST_ASSERT_EQUAL_HEX8(0x55, byte); /* untouched on failure */
}

static void test_fifo_order(void)
{
    for (uint8_t i = 0; i < 10; ++i) TEST_ASSERT_TRUE(ringbuf_put(&rb, i));
    TEST_ASSERT_EQUAL_UINT32(10, ringbuf_count(&rb));
    for (uint8_t i = 0; i < 10; ++i) {
        uint8_t byte;
        TEST_ASSERT_TRUE(ringbuf_get(&rb, &byte));
        TEST_ASSERT_EQUAL_UINT8(i, byte);
    }
    TEST_ASSERT_TRUE(ringbuf_empty(&rb));
}

static void test_full_rejects_without_overwriting(void)
{
    for (uint32_t i = 0; i < RINGBUF_SIZE; ++i) TEST_ASSERT_TRUE(ringbuf_put(&rb, (uint8_t)i));
    TEST_ASSERT_FALSE(ringbuf_put(&rb, 0xEE));
    TEST_ASSERT_EQUAL_UINT32(RINGBUF_SIZE, ringbuf_count(&rb));
    uint8_t byte;
    TEST_ASSERT_TRUE(ringbuf_get(&rb, &byte));
    TEST_ASSERT_EQUAL_UINT8(0, byte);   /* oldest byte survived */
    TEST_ASSERT_TRUE(ringbuf_put(&rb, 0xEE)); /* one slot free again */
}

static void test_index_wraparound(void)
{
    /* Start just below 2^32 so the free-running indices overflow mid-test. */
    rb.head = rb.tail = 0xFFFFFFF0u;
    for (uint32_t round = 0; round < 3; ++round) {
        for (uint32_t i = 0; i < 40; ++i) TEST_ASSERT_TRUE(ringbuf_put(&rb, (uint8_t)(round * 40 + i)));
        TEST_ASSERT_EQUAL_UINT32(40, ringbuf_count(&rb));
        for (uint32_t i = 0; i < 40; ++i) {
            uint8_t byte;
            TEST_ASSERT_TRUE(ringbuf_get(&rb, &byte));
            TEST_ASSERT_EQUAL_UINT8((uint8_t)(round * 40 + i), byte);
        }
    }
    TEST_ASSERT_TRUE(ringbuf_empty(&rb));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_empty);
    RUN_TEST(test_fifo_order);
    RUN_TEST(test_full_rejects_without_overwriting);
    RUN_TEST(test_index_wraparound);
    return UNITY_END();
}
