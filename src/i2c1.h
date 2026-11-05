#ifndef SENSOR_I2C1_H
#define SENSOR_I2C1_H
#include <stdbool.h>
#include <stdint.h>
/* Clock, PB8/PB9 AF4 open-drain, 100 kHz standard mode at PCLK1 = 16 MHz. */
void i2c1_init(void);
/* START, address+W, count data bytes, STOP. False on NACK or timeout. */
bool i2c1_write(uint8_t address7, const uint8_t *data, uint32_t count);
/* Write the register pointer, repeated START, read count bytes (RM0390 24.3.3
 * 1/2/N-byte endings), STOP. False on NACK or timeout. */
bool i2c1_read(uint8_t address7, uint8_t reg, uint8_t *out, uint32_t count);
#endif
