#include "temperature.h"
#include <limits.h>

/* Keep the Bosch 4.2.3 int32 operation order. Reject pathological calibration
 * that would overflow an intermediate instead of depending on signed-C UB.
 * No int64, floating point or libgcc arithmetic is required on Cortex-M4.
 */
static bool multiply(int32_t a, int32_t b, int32_t *result)
{
    uint32_t ua = a < 0 ? 0u - (uint32_t)a : (uint32_t)a;
    uint32_t ub = b < 0 ? 0u - (uint32_t)b : (uint32_t)b;
    if (ub && ua > (uint32_t)INT32_MAX / ub) return false;
    int32_t magnitude = (int32_t)(ua * ub);
    *result = (a < 0) != (b < 0) ? -magnitude : magnitude;
    return true;
}

/* Portable arithmetic shift, including negative values, without an unsigned
 * to signed conversion outside INT32 range. Counts here are always 1..31.
 */
static int32_t asr(int32_t value, unsigned count)
{
    uint32_t shifted = (uint32_t)value >> count;
    if (value < 0) shifted |= ~(UINT32_MAX >> count);
    return (shifted & 0x80000000u) ? -1 - (int32_t)~shifted : (int32_t)shifted;
}

bool sensor_compensate_temperature(uint32_t adc, uint32_t t1, int32_t t2,
                                   int32_t t3, int32_t *temperature)
{
    if (!temperature || adc > 0xFFFFFu || adc == 0x80000u || t1 > 65535u ||
        t2 < -32768 || t2 > 32767 || t3 < -32768 || t3 > 32767) return false;
    int32_t a = (int32_t)(adc >> 3) - (int32_t)(t1 * 2u);
    int32_t b = (int32_t)(adc >> 4) - (int32_t)t1;
    int32_t first, second;
    if (!multiply(a, t2, &first) || !multiply(b, b, &second)) return false;
    first = asr(first, 11);
    if (!multiply(asr(second, 12), t3, &second)) return false;
    second = asr(second, 14);
    if ((second > 0 && first > INT32_MAX - second) ||
        (second < 0 && first < INT32_MIN - second)) return false;
    int32_t fine = first + second;
    if (fine < -1000000 || fine > 1000000) return false;
    int32_t result = asr(fine * 5 + 128, 8);
    if (result < -4000 || result > 8500) return false;
    *temperature = result;
    return true;
}

bool sensor_format_temperature(int32_t value, char output[8])
{
    if (!output || value < -4000 || value > 8500) return false;
    uint32_t magnitude = value < 0 ? 0u - (uint32_t)value : (uint32_t)value;
    unsigned whole = 0, tens = 0, fraction_tens = 0, pos = 0;
    while (magnitude >= 100u) { magnitude -= 100u; ++whole; }
    while (whole >= 10u) { whole -= 10u; ++tens; }
    while (magnitude >= 10u) { magnitude -= 10u; ++fraction_tens; }
    if (value < 0) output[pos++] = '-';
    if (tens) output[pos++] = (char)('0' + tens);
    output[pos++] = (char)('0' + whole);
    output[pos++] = '.';
    output[pos++] = (char)('0' + fraction_tens);
    output[pos++] = (char)('0' + magnitude);
    output[pos] = '\0';
    return true;
}
