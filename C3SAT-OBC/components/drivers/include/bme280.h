/**
 * @file bme280.h
 * @brief BME280 temperature/pressure/humidity — models the thermal sensor.
 *
 * Implements the full Bosch fixed-point compensation. Falls back to a slow
 * thermal sine simulation when absent, so the heater autonomy can be exercised.
 */
#ifndef BME280_H
#define BME280_H

#include <stdbool.h>
#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temperature_c;
    float pressure_hpa;
    float humidity_pct;
} bme280_sample_t;

obc_err_t bme280_init(void);
bool      bme280_present(void);
obc_err_t bme280_read(bme280_sample_t *out);

#ifdef __cplusplus
}
#endif

#endif /* BME280_H */
