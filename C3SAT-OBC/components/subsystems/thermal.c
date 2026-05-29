#include "subsystems.h"
#include "obc_state.h"
#include "obc_config.h"
#include "fdir.h"
#include "bme280.h"
#include "ds3231.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "thermal";

/* Manual heater override from ground (TC_OP_SET_HEATER). -1 = autonomous. */
static volatile int s_heater_override = -1;

void thermal_set_heater_override(int on) { s_heater_override = on; }

static void thermal_task(void *arg)
{
    (void) arg;
    fdir_set_deadline(OBC_SS_THERMAL, 3000);
    obc_state_signal(EVT_THERMAL_READY);

    bool heater = false;
    TickType_t next = xTaskGetTickCount();
    for (;;) {
        bme280_sample_t b;
        thermal_telemetry_t t = {0};

        if (bme280_read(&b) == OBC_OK) {
            t.temperature_c = b.temperature_c;
            t.pressure_hpa  = b.pressure_hpa;
            t.humidity_pct  = b.humidity_pct;
            ds3231_get_temp(&t.mcu_temp_c);

            /* Heater hysteresis: on below -5 °C, off above +5 °C. */
            if (s_heater_override >= 0) {
                heater = (s_heater_override != 0);
            } else if (t.temperature_c < THERMAL_HEATER_ON_C) {
                heater = true;
            } else if (t.temperature_c > THERMAL_HEATER_OFF_C) {
                heater = false;
            }
            t.heater_on = heater;

            if (t.temperature_c > THERMAL_HOT_C) {
                obc_state_raise_fault(OBC_FAULT_THERMAL_HOT);
            } else {
                obc_state_clear_fault(OBC_FAULT_THERMAL_HOT);
            }
            if (t.temperature_c < THERMAL_COLD_C) {
                obc_state_raise_fault(OBC_FAULT_THERMAL_COLD);
            } else {
                obc_state_clear_fault(OBC_FAULT_THERMAL_COLD);
            }
            obc_state_set_thermal(&t);
        } else {
            obc_state_raise_fault(OBC_FAULT_SENSOR_LOSS);
        }

        fdir_heartbeat(OBC_SS_THERMAL);
        vTaskDelayUntil(&next, pdMS_TO_TICKS(PERIOD_THERMAL_MS));
    }
}

obc_err_t thermal_start(void)
{
    if (bme280_init() != OBC_OK) {
        ESP_LOGW(TAG, "BME280 init issue (continuing)");
    }
    if (xTaskCreate(thermal_task, "thermal", STACK_THERMAL, NULL,
                    PRIO_THERMAL, NULL) != pdPASS) {
        return OBC_ERR_NO_MEM;
    }
    return OBC_OK;
}
