#include "mpu6050.h"
#include "hal_i2c.h"
#include "bsp_pins.h"

#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "mpu6050";

#define REG_PWR_MGMT_1  0x6B
#define REG_WHO_AM_I    0x75
#define REG_ACCEL_XOUT  0x3B  /* 14 bytes: accel(6) temp(2) gyro(6) */
#define WHO_AM_I_VAL    0x68

#define ACCEL_LSB_PER_G   16384.0f  /* ±2g range */
#define GYRO_LSB_PER_DPS  131.0f    /* ±250 dps range */

static bool s_present;

obc_err_t mpu6050_init(void)
{
    uint8_t who = 0;
    if (hal_i2c_probe(BSP_I2C_ADDR_MPU6050) != OBC_OK ||
        hal_i2c_read_u8(BSP_I2C_ADDR_MPU6050, REG_WHO_AM_I, &who) != OBC_OK ||
        who != WHO_AM_I_VAL) {
        s_present = false;
        ESP_LOGW(TAG, "not found -> simulated ADCS");
        return OBC_OK;
    }
    /* Wake from sleep, select gyro PLL clock. */
    hal_i2c_write_u8(BSP_I2C_ADDR_MPU6050, REG_PWR_MGMT_1, 0x01);
    s_present = true;
    ESP_LOGI(TAG, "MPU6050 online");
    return OBC_OK;
}

bool mpu6050_present(void) { return s_present; }

/* Simulate a CubeSat released tumbling, then magnetorquers detumble it: rates
 * decay exponentially toward zero with a little residual wobble. */
static void simulate(mpu6050_sample_t *out)
{
    double t = (double) esp_timer_get_time() / 1e6;
    float decay = expf(-(float) t / 40.0f);
    out->gyro_dps[0] = 60.0f * decay * sinf((float) t * 1.1f) + 0.4f;
    out->gyro_dps[1] = 45.0f * decay * cosf((float) t * 0.9f) - 0.3f;
    out->gyro_dps[2] = 30.0f * decay * sinf((float) t * 1.7f) + 0.2f;
    out->accel_g[0]  = 0.02f * sinf((float) t);
    out->accel_g[1]  = 0.02f * cosf((float) t);
    out->accel_g[2]  = 1.0f; /* nadir-ish */
    out->temp_c      = 24.0f;
}

obc_err_t mpu6050_read(mpu6050_sample_t *out)
{
    if (out == NULL) {
        return OBC_ERR_INVALID_ARG;
    }
    if (!s_present) {
        simulate(out);
        return OBC_OK;
    }
    uint8_t b[14];
    if (hal_i2c_read_reg(BSP_I2C_ADDR_MPU6050, REG_ACCEL_XOUT, b, sizeof b) != OBC_OK) {
        return OBC_ERR_FAIL;
    }
    int16_t ax = (b[0] << 8) | b[1];
    int16_t ay = (b[2] << 8) | b[3];
    int16_t az = (b[4] << 8) | b[5];
    int16_t tr = (b[6] << 8) | b[7];
    int16_t gx = (b[8] << 8) | b[9];
    int16_t gy = (b[10] << 8) | b[11];
    int16_t gz = (b[12] << 8) | b[13];
    out->accel_g[0] = ax / ACCEL_LSB_PER_G;
    out->accel_g[1] = ay / ACCEL_LSB_PER_G;
    out->accel_g[2] = az / ACCEL_LSB_PER_G;
    out->gyro_dps[0] = gx / GYRO_LSB_PER_DPS;
    out->gyro_dps[1] = gy / GYRO_LSB_PER_DPS;
    out->gyro_dps[2] = gz / GYRO_LSB_PER_DPS;
    out->temp_c = (tr / 340.0f) + 36.53f;
    return OBC_OK;
}
