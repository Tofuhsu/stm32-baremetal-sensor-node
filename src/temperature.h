#ifndef SENSOR_TEMPERATURE_H
#define SENSOR_TEMPERATURE_H
#include <stdbool.h>
#include <stdint.h>
bool sensor_compensate_temperature(uint32_t adc, uint32_t t1, int32_t t2,
                                   int32_t t3, int32_t *centi_celsius);
bool sensor_format_temperature(int32_t centi_celsius, char output[8]);
#endif
