#include "ssd1306.h"
#include "i2c1.h"

/* I2C control byte (SSD1306 datasheet, I2C write mode): Co = 0, D/C# selects
 * a command stream (0x00) or a data stream (0x40). Section numbers not checked. */
#define CONTROL_COMMANDS 0x00u
#define CONTROL_DATA     0x40u

static uint8_t frame[SSD1306_PAGES][SSD1306_WIDTH];

/* 5x7 glyphs, one byte per column, bit 0 at the top. Only the characters the
 * firmware prints are included. */
typedef struct { char c; uint8_t columns[5]; } glyph_t;
static const glyph_t font[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}}, {'-', {0x08, 0x08, 0x08, 0x08, 0x08}},
    {'.', {0x00, 0x60, 0x60, 0x00, 0x00}}, {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}}, {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
    {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}}, {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}}, {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}}, {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}}, {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
    {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}}, {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}}, {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}}, {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
    {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
};

static const uint8_t *glyph(char c)
{
    for (uint32_t i = 0; i < sizeof font / sizeof font[0]; ++i)
        if (font[i].c == c) return font[i].columns;
    return font[0].columns;
}

static bool commands(const uint8_t *bytes, uint32_t count)
{
    uint8_t buffer[32];
    if (count + 1u > sizeof buffer) return false;
    buffer[0] = CONTROL_COMMANDS;
    for (uint32_t i = 0; i < count; ++i) buffer[i + 1u] = bytes[i];
    return i2c1_write(SSD1306_ADDRESS, buffer, count + 1u);
}

bool ssd1306_init(void)
{
    /* Common power-on sequence for a 128x64 module with the internal charge
     * pump (8D 14); command meanings from the SSD1306 command table. Not yet
     * checked on a real module. */
    static const uint8_t sequence[] = {
        0xAE,       /* display off */
        0xD5, 0x80, /* clock divide ratio / oscillator */
        0xA8, 0x3F, /* multiplex ratio: 64 rows */
        0xD3, 0x00, /* display offset 0 */
        0x40,       /* start line 0 */
        0x8D, 0x14, /* charge pump on */
        0x20, 0x00, /* horizontal addressing */
        0xA1,       /* segment remap: column 127 -> SEG0 */
        0xC8,       /* COM scan direction: remapped */
        0xDA, 0x12, /* COM pins: alternative, no remap */
        0x81, 0xCF, /* contrast */
        0xD9, 0xF1, /* pre-charge period */
        0xDB, 0x40, /* VCOMH deselect level */
        0xA4,       /* display follows RAM */
        0xA6,       /* normal (not inverted) */
        0xAF,       /* display on */
    };
    return commands(sequence, sizeof sequence);
}

void ssd1306_clear(void)
{
    for (uint32_t page = 0; page < SSD1306_PAGES; ++page)
        for (uint32_t column = 0; column < SSD1306_WIDTH; ++column)
            frame[page][column] = 0u;
}

void ssd1306_text(uint8_t page, uint8_t column, const char *text)
{
    if (page >= SSD1306_PAGES) return;
    for (uint32_t x = column; *text != '\0'; ++text) {
        const uint8_t *columns = glyph(*text);
        for (uint32_t i = 0; i < 6u; ++i, ++x) {
            if (x >= SSD1306_WIDTH) return;
            frame[page][x] = i < 5u ? columns[i] : 0u;
        }
    }
}

bool ssd1306_flush(uint8_t first, uint8_t last)
{
    if (first > last || last >= SSD1306_PAGES) return false;
    /* Column range 0..127, page range first..last; the address pointer then
     * advances through the window in horizontal mode. */
    const uint8_t window[] = {0x21, 0x00, SSD1306_WIDTH - 1u, 0x22, first, last};
    if (!commands(window, sizeof window)) return false;
    uint8_t buffer[1u + SSD1306_WIDTH];
    buffer[0] = CONTROL_DATA;
    for (uint32_t page = first; page <= last; ++page) {
        for (uint32_t column = 0; column < SSD1306_WIDTH; ++column)
            buffer[column + 1u] = frame[page][column];
        if (!i2c1_write(SSD1306_ADDRESS, buffer, sizeof buffer)) return false;
    }
    return true;
}

const uint8_t *ssd1306_framebuffer(void)
{
    return &frame[0][0];
}
