#include "subsystems.h"
#include "obc_state.h"
#include "obc_config.h"
#include "fdir.h"
#include "ina219.h"

#include "esp_adc/adc_oneshot.h"
#include "bsp_pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "eps";

/* Optional bench knob: a potentiometer on the solar-input ADC lets you drive
 * the *simulated* bus voltage by hand, so you can deliberately push the EPS
 * into under-voltage and watch FDIR safe the spacecraft. Only used when no real
 * INA219 is fitted. */
static adc_oneshot_unit_handle_t s_adc;

static bool adc_bus_voltage(float *v)
{
    if (s_adc == NULL) {
        return false;
    }
    int raw = 0;
    if (adc_oneshot_read(s_adc, BSP_ADC_SOLAR_CH, &raw) != ESP_OK) {
        return false;
    }
    /* 0..4095 (~0..3.3 V at 12 dB atten) -> battery range 3.0..4.2 V. */
    float frac = raw / 4095.0f;
    *v = 3.0f + frac * 1.2f;
    return true;
}

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
            /* If running simulated, let the bench potentiometer (if present)
             * override the bus voltage for hands-on fault injection. */
            float vbus = s.bus_voltage_v;
            if (!ina219_present()) {
                float pot_v;
                if (adc_bus_voltage(&pot_v)) {
                    vbus = pot_v;
                    s.shunt_current_ma = 150.0f;        /* nominal load */
                    s.power_mw = vbus * s.shunt_current_ma;
                }
            }
            t.bus_voltage_v  = vbus;
            t.bus_current_ma = s.shunt_current_ma;
            t.power_mw       = s.power_mw;
            t.battery_soc_pct = voltage_to_soc(vbus);
            t.charging = (s.shunt_current_ma < 0.0f); /* sign convention: in = charge */

            /* Fault detection with hysteresis via the recover threshold. */
            if (vbus < EPS_UV_THRESHOLD_V) {
                obc_state_raise_fault(OBC_FAULT_EPS_UV);
            } else if (vbus > EPS_SAFE_RECOVER_V) {
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

static void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
    if (adc_oneshot_new_unit(&unit_cfg, &s_adc) != ESP_OK) {
        s_adc = NULL;
        return;
    }
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_oneshot_config_channel(s_adc, BSP_ADC_SOLAR_CH, &chan_cfg) != ESP_OK) {
        adc_oneshot_del_unit(s_adc);
        s_adc = NULL;
    }
}

obc_err_t eps_start(void)
{
    if (ina219_init() != OBC_OK) {
        ESP_LOGW(TAG, "INA219 init issue (continuing)");
    }
    if (!ina219_present()) {
        adc_init();  /* enable the bench fault-injection knob */
    }
    if (xTaskCreate(eps_task, "eps", STACK_EPS, NULL, PRIO_EPS, NULL) != pdPASS) {
        return OBC_ERR_NO_MEM;
    }
    return OBC_OK;
}
