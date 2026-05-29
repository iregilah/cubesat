/**
 * @file ina219.h
 * @brief INA219 bus voltage/current monitor — models the EPS power sensor.
 *
 * If no INA219 is detected on the bus the driver transparently falls back to a
 * physically plausible simulation (a slowly draining/charging battery driven
 * by the ADC "solar" input), so the EPS subsystem and its autonomy logic can
 * be exercised on a bare DevKit.
 */
#ifndef INA219_H
#define INA219_H

#include <stdbool.h>
#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float bus_voltage_v;
    float shunt_current_ma;
    float power_mw;
} ina219_sample_t;

/** Probe + configure the INA219. Sets simulation mode if absent. */
obc_err_t ina219_init(void);

/** True if a real INA219 answered on the bus. */
bool ina219_present(void);

/** Read one sample (real or simulated). */
obc_err_t ina219_read(ina219_sample_t *out);

#ifdef __cplusplus
}
#endif

#endif /* INA219_H */
