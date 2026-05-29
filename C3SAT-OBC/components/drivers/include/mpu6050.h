/**
 * @file mpu6050.h
 * @brief MPU6050 6-axis IMU — models the ADCS attitude sensor.
 *
 * Falls back to a simulated "tumbling then detumbling" body-rate profile when
 * no IMU is present, so the ADCS control loop and tumble FDIR can be shown off
 * without hardware.
 */
#ifndef MPU6050_H
#define MPU6050_H

#include <stdbool.h>
#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float gyro_dps[3];   /**< Angular rate X/Y/Z [deg/s]. */
    float accel_g[3];    /**< Acceleration X/Y/Z [g]. */
    float temp_c;        /**< Die temperature [°C]. */
} mpu6050_sample_t;

obc_err_t mpu6050_init(void);
bool      mpu6050_present(void);
obc_err_t mpu6050_read(mpu6050_sample_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H */
