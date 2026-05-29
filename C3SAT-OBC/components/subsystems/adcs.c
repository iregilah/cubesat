#include "subsystems.h"
#include "obc_state.h"
#include "obc_config.h"
#include "fdir.h"
#include "mpu6050.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "adcs";

#define DEG (180.0f / 3.14159265f)

static void adcs_task(void *arg)
{
    (void) arg;
    fdir_set_deadline(OBC_SS_ADCS, 1000);
    obc_state_signal(EVT_ADCS_READY);

    float roll = 0, pitch = 0, yaw = 0;
    const float dt = PERIOD_ADCS_MS / 1000.0f;

    TickType_t next = xTaskGetTickCount();
    for (;;) {
        mpu6050_sample_t m;
        adcs_telemetry_t t = {0};

        if (mpu6050_read(&m) == OBC_OK) {
            /* Complementary filter: integrate gyro, nudge toward the accel
             * tilt estimate to bound drift. Yaw integrates from gyro only. */
            float acc_roll  = atan2f(m.accel_g[1], m.accel_g[2]) * DEG;
            float acc_pitch = atan2f(-m.accel_g[0],
                                     sqrtf(m.accel_g[1] * m.accel_g[1] +
                                           m.accel_g[2] * m.accel_g[2])) * DEG;
            roll  = 0.98f * (roll  + m.gyro_dps[0] * dt) + 0.02f * acc_roll;
            pitch = 0.98f * (pitch + m.gyro_dps[1] * dt) + 0.02f * acc_pitch;
            yaw  += m.gyro_dps[2] * dt;
            if (yaw > 180.0f)  yaw -= 360.0f;
            if (yaw < -180.0f) yaw += 360.0f;

            float rate_rms = sqrtf((m.gyro_dps[0] * m.gyro_dps[0] +
                                    m.gyro_dps[1] * m.gyro_dps[1] +
                                    m.gyro_dps[2] * m.gyro_dps[2]) / 3.0f);

            for (int i = 0; i < 3; ++i) {
                t.gyro_dps[i] = m.gyro_dps[i];
                t.accel_g[i]  = m.accel_g[i];
            }
            t.euler_deg[0] = roll;
            t.euler_deg[1] = pitch;
            t.euler_deg[2] = yaw;
            t.rate_rms_dps = rate_rms;
            t.detumbled    = (rate_rms < ADCS_DETUMBLE_DPS);

            if (rate_rms > ADCS_TUMBLE_DPS) {
                obc_state_raise_fault(OBC_FAULT_ADCS_TUMBLE);
            } else if (t.detumbled) {
                obc_state_clear_fault(OBC_FAULT_ADCS_TUMBLE);
            }
            obc_state_set_adcs(&t);
        } else {
            obc_state_raise_fault(OBC_FAULT_SENSOR_LOSS);
        }

        fdir_heartbeat(OBC_SS_ADCS);
        vTaskDelayUntil(&next, pdMS_TO_TICKS(PERIOD_ADCS_MS));
    }
}

obc_err_t adcs_start(void)
{
    if (mpu6050_init() != OBC_OK) {
        ESP_LOGW(TAG, "MPU6050 init issue (continuing)");
    }
    if (xTaskCreate(adcs_task, "adcs", STACK_ADCS, NULL, PRIO_ADCS, NULL) != pdPASS) {
        return OBC_ERR_NO_MEM;
    }
    return OBC_OK;
}
