/* SSD1306 128x64 OLED on I2C1 at 0x3C (SA0 low), horizontal addressing.
 * The driver keeps a 1 KiB frame buffer and sends only the pages asked for.
 */
#ifndef SENSOR_SSD1306_H
#define SENSOR_SSD1306_H
#include <stdbool.h>
#include <stdint.h>

#define SSD1306_ADDRESS 0x3Cu
#define SSD1306_WIDTH 128u
#define SSD1306_PAGES 8u

/* i2c1_init() must have been called. False if the display does not ACK. */
bool ssd1306_init(void);
void ssd1306_clear(void);
/* Draw text in the 5x7 font, 6 columns per character, on one 8-row page.
 * Characters without a glyph are drawn as blanks; text past column 127 is cut. */
void ssd1306_text(uint8_t page, uint8_t column, const char *text);
/* Send pages first..last (inclusive) of the frame buffer. */
bool ssd1306_flush(uint8_t first, uint8_t last);
/* Frame buffer, page-major: [page][column], bit 0 = top row of the page. */
const uint8_t *ssd1306_framebuffer(void);
#endif
