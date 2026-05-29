#include "subsystems.h"
#include "obc_state.h"
#include "obc_config.h"
#include "fdir.h"
#include "ina219.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "eps";

/* Map battery terminal voltage to a coarse state-of-charge for a Li-ion cell. */
static float voltage_to_soc(float v)
{
    float soc = (v - 3.0f) / (4.2f - 3.0f) * 100.0f;
    if (soc < 0.0f) soc = 0.0f;
    if (soc > 100.0f) soc = 100.0f;
    return soc;
}

/* Shed non-essential loads as the battery depletes (a real EPS autonomy rule).
 * Level 0: all on. 1: payload off. 2: + non-critical heaters. 3: survival only. */
static uint8_t load_shed_level(float soc, uint16_t faults)
{
    if (faults & OBC_FAULT_EPS_UV) return 3;
    if (soc < 20.0f) return 2;
    if (soc < 40.0f) return 1;
    return 0;
}

static void eps_task(void *arg)
{
    (void) arg;
    fdir_set_deadline(OBC_SS_EPS, 2000);
    obc_state_signal(EVT_EPS_READY);

    TickType_t next = xTaskGetTickCount();
    for (;;) {
        ina219_sample_t s;
        eps_telemetry_t t = {0};

        if (ina219_read(&s) == OBC_OK) {
            t.bus_voltage_v  = s.bus_voltage_v;
            t.bus_current_ma = s.shunt_current_ma;
            t.power_mw       = s.power_mw;
            t.battery_soc_pct = voltage_to_soc(s.bus_voltage_v);
            t.charging = (s.shunt_current_ma < 0.0f); /* sign convention: in = charge */

            /* Fault detection with hysteresis via the recover threshold. */
            if (s.bus_voltage_v < EPS_UV_THRESHOLD_V) {
                obc_state_raise_fault(OBC_FAULT_EPS_UV);
            } else if (s.bus_voltage_v > EPS_SAFE_RECOVER_V) {
                obc_state_clear_fault(OBC_FAULT_EPS_UV);
            }
            if (s.shunt_current_ma > EPS_OC_THRESHOLD_MA) {
                obc_state_raise_fault(OBC_FAULT_EPS_OC);
            } else {
                obc_state_clear_fault(OBC_FAULT_EPS_OC);
            }

            t.load_shed_level = load_shed_level(t.battery_soc_pct, obc_state_faults());
            obc_state_set_eps(&t);
        } else {
            obc_state_raise_fault(OBC_FAULT_SENSOR_LOSS);
        }

        fdir_heartbeat(OBC_SS_EPS);
        vTaskDelayUntil(&next, pdMS_TO_TICKS(PERIOD_EPS_MS));
    }
}

obc_err_t eps_start(void)
{
    if (ina219_init() != OBC_OK) {
        ESP_LOGW(TAG, "INA219 init issue (continuing)");
    }
    if (xTaskCreate(eps_task, "eps", STACK_EPS, NULL, PRIO_EPS, NULL) != pdPASS) {
        return OBC_ERR_NO_MEM;
    }
    return OBC_OK;
}
