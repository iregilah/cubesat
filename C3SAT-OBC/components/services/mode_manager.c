#include "mode_manager.h"
#include "obc_state.h"
#include "obc_config.h"
#include "fdir.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "mode";

/* Faults that force the spacecraft out of NOMINAL/PAYLOAD into SAFE. */
#define CRITICAL_FAULTS (OBC_FAULT_EPS_UV | OBC_FAULT_EPS_OC | \
                         OBC_FAULT_THERMAL_HOT | OBC_FAULT_WATCHDOG)

static void evaluate(void)
{
    obc_mode_t mode    = obc_state_mode();
    uint16_t   faults  = obc_state_faults();
    eps_telemetry_t eps;  obc_state_get_eps(&eps);
    adcs_telemetry_t adcs; obc_state_get_adcs(&adcs);

    bool critical = (faults & CRITICAL_FAULTS) != 0;

    switch (mode) {
        case OBC_MODE_NOMINAL:
        case OBC_MODE_PAYLOAD:
            if (critical) {
                ESP_LOGW(TAG, "critical fault 0x%02X -> SAFE", faults & CRITICAL_FAULTS);
                obc_state_set_mode(OBC_MODE_SAFE);
            }
            break;

        case OBC_MODE_SAFE:
            /* Auto-recover to NOMINAL once the bus is healthy and we're not
             * tumbling. PAYLOAD is only ever entered by ground command. */
            if (!critical &&
                eps.bus_voltage_v >= EPS_SAFE_RECOVER_V &&
                adcs.detumbled) {
                obc_state_set_mode(OBC_MODE_NOMINAL);
            }
            break;

        case OBC_MODE_BOOT:
            /* app_main moves us out of BOOT after the startup barrier. */
            obc_state_set_mode(OBC_MODE_SAFE);
            break;

        case OBC_MODE_FAULT:
            if (!critical) {
                obc_state_set_mode(OBC_MODE_SAFE);
            }
            break;

        default:
            break;
    }
}

static void mode_task(void *arg)
{
    (void) arg;
    for (;;) {
        evaluate();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

obc_err_t mode_manager_init(void) { return OBC_OK; }

obc_err_t mode_manager_start(void)
{
    if (xTaskCreate(mode_task, "mode_mgr", STACK_MODE_MGR, NULL,
                    PRIO_MODE_MGR, NULL) != pdPASS) {
        return OBC_ERR_NO_MEM;
    }
    return OBC_OK;
}
