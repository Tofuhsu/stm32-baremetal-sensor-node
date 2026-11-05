#include "bme280.h"
#include "temperature.h"
#include "i2c1.h"

#define BME280_ADDRESS 0x76u

static uint32_t t1;
static int32_t t2, t3;

static bool write_register(uint8_t reg, uint8_t value)
{
    const uint8_t bytes[2] = {reg, value};
    return i2c1_write(BME280_ADDRESS, bytes, 2);
}

static uint32_t u16(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] * 256u;
}
static int32_t s16(const uint8_t *p)
{
    uint32_t value = u16(p);
    return value < 32768u ? (int32_t)value : (int32_t)value - 65536;
}

/* i2c1_init() must have been called. */
bool bme280_init(void)
{
    uint8_t id, cal[6];
    if (!i2c1_read(BME280_ADDRESS, 0xD0, &id, 1) || id != 0x60u ||
        !i2c1_read(BME280_ADDRESS, 0x88, cal, sizeof cal)) return false;
    /* Driver policy, not a Bosch-specified validity bit: a complete all-zero
     * trimming block is rejected instead of reporting plausible 0.00 C from
     * a failed register-pointer transfer. A real zero-temperature sample
     * with valid trimming parameters is still accepted.
     */
    if ((u16(cal) | u16(cal + 2) | u16(cal + 4)) == 0u) return false;
    t1 = u16(cal); t2 = s16(cal + 2); t3 = s16(cal + 4);
    /* Bosch 5.4.3/5.4.5: ctrl_hum first; ctrl_meas latches it. T/H x1,
     * pressure skipped. Normal mode lets host-side temperature changes appear.
     */
    return write_register(0xF2, 1) && write_register(0xF4, 0x23);
}

bool bme280_read_temperature(int32_t *centi_celsius)
{
    uint8_t raw[3];
    if (!i2c1_read(BME280_ADDRESS, 0xFA, raw, 3)) return false;
    uint32_t adc = (uint32_t)raw[0] * 4096u | (uint32_t)raw[1] * 16u | raw[2] / 16u;
    if (adc == 0x80000u) return false;
    return sensor_compensate_temperature(adc, t1, t2, t3, centi_celsius);
}
