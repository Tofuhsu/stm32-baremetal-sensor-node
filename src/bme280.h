#ifndef SENSOR_BME280_H
#define SENSOR_BME280_H
#include <stdbool.h>
#include <stdint.h>
bool bme280_init(void);
bool bme280_read_temperature(int32_t *centi_celsius);
#endif
