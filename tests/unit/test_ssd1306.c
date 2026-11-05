/* Unity tests for the SSD1306 driver with a fake I2C1 that records writes. */
#include <string.h>
#include "unity.h"
#include "../../src/i2c1.h"
#include "../../src/ssd1306.h"

#define MAX_TRANSFERS 16
static struct { uint8_t address; uint8_t data[200]; uint32_t count; } transfers[MAX_TRANSFERS];
static uint32_t transfer_count;
static bool device_present;

void i2c1_init(void) {}
bool i2c1_read(uint8_t address7, uint8_t reg, uint8_t *out, uint32_t count)
{
    (void)address7; (void)reg; (void)out; (void)count;
    return false;
}
bool i2c1_write(uint8_t address7, const uint8_t *data, uint32_t count)
{
    if (!device_present) return false;
    TEST_ASSERT_TRUE(transfer_count < MAX_TRANSFERS);
    TEST_ASSERT_TRUE(count <= sizeof transfers[0].data);
    transfers[transfer_count].address = address7;
    memcpy(transfers[transfer_count].data, data, count);
    transfers[transfer_count].count = count;
    ++transfer_count;
    return true;
}

void setUp(void)
{
    transfer_count = 0;
    device_present = true;
    ssd1306_clear();
}
void tearDown(void) {}

static void test_init_sends_one_command_stream(void)
{
    TEST_ASSERT_TRUE(ssd1306_init());
    TEST_ASSERT_EQUAL_UINT32(1, transfer_count);
    TEST_ASSERT_EQUAL_HEX8(SSD1306_ADDRESS, transfers[0].address);
    TEST_ASSERT_EQUAL_HEX8(0x00, transfers[0].data[0]);            /* command stream */
    TEST_ASSERT_EQUAL_HEX8(0xAE, transfers[0].data[1]);            /* display off first */
    TEST_ASSERT_EQUAL_HEX8(0xAF, transfers[0].data[transfers[0].count - 1]); /* on last */
    /* charge pump enabled: 8D 14 */
    bool pump = false;
    for (uint32_t i = 1; i + 1 < transfers[0].count; ++i)
        if (transfers[0].data[i] == 0x8D && transfers[0].data[i + 1] == 0x14) pump = true;
    TEST_ASSERT_TRUE(pump);
}

static void test_missing_display_reports_false(void)
{
    device_present = false;
    TEST_ASSERT_FALSE(ssd1306_init());
    TEST_ASSERT_FALSE(ssd1306_flush(0, 7));
}

static void test_text_renders_glyph_columns(void)
{
    ssd1306_text(2, 6, "1.");
    const uint8_t *fb = ssd1306_framebuffer();
    const uint8_t one[6] = {0x00, 0x42, 0x7F, 0x40, 0x00, 0x00};
    const uint8_t dot[6] = {0x00, 0x60, 0x60, 0x00, 0x00, 0x00};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(one, fb + 2 * SSD1306_WIDTH + 6, 6);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(dot, fb + 2 * SSD1306_WIDTH + 12, 6);
    TEST_ASSERT_EQUAL_HEX8(0, fb[2 * SSD1306_WIDTH + 5]);                       /* left of text */
    TEST_ASSERT_EQUAL_HEX8(0, fb[1 * SSD1306_WIDTH + 7]);                       /* other page */
}

static void test_unknown_character_is_blank_and_text_is_clipped(void)
{
    ssd1306_text(0, 120, "x8");           /* 'x' has no glyph; '8' would start at 126 */
    const uint8_t *fb = ssd1306_framebuffer();
    for (uint32_t c = 120; c < 126; ++c) TEST_ASSERT_EQUAL_HEX8(0, fb[c]);
    TEST_ASSERT_EQUAL_HEX8(0x36, fb[126]);
    TEST_ASSERT_EQUAL_HEX8(0x49, fb[127]); /* cut after column 127 */
}

static void test_flush_sets_window_then_sends_pages(void)
{
    ssd1306_text(3, 0, "8");
    TEST_ASSERT_TRUE(ssd1306_flush(3, 4));
    TEST_ASSERT_EQUAL_UINT32(3, transfer_count);
    const uint8_t window[] = {0x00, 0x21, 0x00, 0x7F, 0x22, 3, 4};
    TEST_ASSERT_EQUAL_UINT32(sizeof window, transfers[0].count);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(window, transfers[0].data, sizeof window);
    TEST_ASSERT_EQUAL_UINT32(1 + SSD1306_WIDTH, transfers[1].count);
    TEST_ASSERT_EQUAL_HEX8(0x40, transfers[1].data[0]);               /* data stream */
    TEST_ASSERT_EQUAL_HEX8(0x36, transfers[1].data[1]);               /* page 3, column 0 */
    TEST_ASSERT_EQUAL_HEX8(0x00, transfers[2].data[1]);               /* page 4 is empty */
    TEST_ASSERT_FALSE(ssd1306_flush(5, 4));
    TEST_ASSERT_FALSE(ssd1306_flush(0, 8));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_sends_one_command_stream);
    RUN_TEST(test_missing_display_reports_false);
    RUN_TEST(test_text_renders_glyph_columns);
    RUN_TEST(test_unknown_character_is_blank_and_text_is_clipped);
    RUN_TEST(test_flush_sets_window_then_sends_pages);
    return UNITY_END();
}
