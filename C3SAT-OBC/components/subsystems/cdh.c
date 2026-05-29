#include "subsystems.h"
#include "obc_state.h"
#include "obc_config.h"
#include "telemetry.h"
#include "telecommand.h"
#include "storage.h"
#include "clock_svc.h"
#include "fdir.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "cdh";

static void dispatch(const telecommand_t *cmd)
{
    obc_state_post_event(OBC_SS_CDH, "RX op=0x%02X seq=%u", cmd->opcode, cmd->seq);
    switch (cmd->opcode) {
        case TC_OP_NOP:
            break;
        case TC_OP_PING:
            obc_state_post_event(OBC_SS_CDH, "PONG");
            break;
        case TC_OP_SET_MODE:
            if (cmd->len >= 1 && cmd->payload[0] < OBC_MODE_COUNT) {
                /* Ground requests a mode; mode_manager keeps veto authority. */
                obc_state_set_mode((obc_mode_t) cmd->payload[0]);
            }
            break;
        case TC_OP_REQ_BEACON: {
            obc_beacon_t b;
            obc_state_get_beacon(&b);
            telemetry_downlink(&b);
            break;
        }
        case TC_OP_CLEAR_FAULTS:
            for (int bit = 0; bit < 8; ++bit) {
                obc_state_clear_fault((obc_fault_t) (1u << bit));
            }
            obc_state_post_event(OBC_SS_CDH, "faults cleared");
            break;
        case TC_OP_SET_HEATER:
            thermal_set_heater_override(cmd->len >= 1 ? cmd->payload[0] : -1);
            break;
        case TC_OP_DUMP_LOG:
            storage_dump();
            break;
        case TC_OP_REBOOT:
            obc_state_post_event(OBC_SS_CDH, "REBOOT commanded");
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
            break;
        default:
            ESP_LOGW(TAG, "unknown opcode 0x%02X", cmd->opcode);
            break;
    }
}

static void cdh_task(void *arg)
{
    (void) arg;
    fdir_set_deadline(OBC_SS_CDH, 4000);
    obc_state_signal(EVT_CDH_READY);

    QueueHandle_t cmd_q = obc_state_command_queue();
    TickType_t last_beacon = xTaskGetTickCount();

    for (;;) {
        /* Block for a command, but wake at least often enough to keep the
         * beacon cadence and the heartbeat alive. */
        telecommand_t cmd;
        if (xQueueReceive(cmd_q, &cmd, pdMS_TO_TICKS(200)) == pdTRUE) {
            dispatch(&cmd);
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_beacon) >= pdMS_TO_TICKS(PERIOD_BEACON_MS)) {
            last_beacon = now;
            obc_beacon_t b;
            obc_state_get_beacon(&b);
            b.mission_time_s = clock_svc_uptime_s();
            telemetry_downlink(&b);
            ESP_LOGI(TAG, "beacon: mode=%d V=%.2f SoC=%.0f%% T=%.1fC rate=%.1f f=0x%02X",
                     b.mode, b.eps.bus_voltage_v, b.eps.battery_soc_pct,
                     b.thermal.temperature_c, b.adcs.rate_rms_dps, b.fault_flags);
        }

        fdir_heartbeat(OBC_SS_CDH);
    }
}

obc_err_t cdh_start(void)
{
    if (xTaskCreate(cdh_task, "cdh", STACK_CDH, NULL, PRIO_CDH, NULL) != pdPASS) {
        return OBC_ERR_NO_MEM;
    }
    return OBC_OK;
}
