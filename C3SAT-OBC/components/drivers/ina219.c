#include "ina219.h"
#include "hal_i2c.h"
#include "bsp_pins.h"

#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "ina219";

#define REG_CONFIG       0x00
#define REG_SHUNT        0x01
#define REG_BUS          0x02
#define REG_POWER        0x03
#define REG_CURRENT      0x04
#define REG_CALIBRATION  0x05

static bool  s_present;
/* Calibration scale factors for a 0.1 ohm shunt, 32V/2A range (datasheet). */
static const float CURRENT_LSB_MA = 0.1f;   /* 100 µA per bit */
static const float POWER_LSB_MW   = 2.0f;   /* 20 * current LSB */

static obc_err_t write_u16_be(uint8_t reg, uint16_t v)
{
    uint8_t b[2] = { v >> 8, v & 0xFF };
    return hal_i2c_write_reg(BSP_I2C_ADDR_INA219, reg, b, 2);
}

obc_err_t ina219_init(void)
{
    if (hal_i2c_probe(BSP_I2C_ADDR_INA219) != OBC_OK) {
        s_present = false;
        ESP_LOGW(TAG, "not found @0x%02X -> simulated EPS", BSP_I2C_ADDR_INA219);
        return OBC_OK;
    }
    /* 32V range, /8 gain, 12-bit ADC, continuous shunt+bus. */
    write_u16_be(REG_CONFIG, 0x399F);
    /* Calibration = 0.04096 / (current_LSB[A] * Rshunt[ohm]). */
    write_u16_be(REG_CALIBRATION, 4096);
    s_present = true;
    ESP_LOGI(TAG, "INA219 online @0x%02X", BSP_I2C_ADDR_INA219);
    return OBC_OK;
}

bool ina219_present(void) { return s_present; }

/* --- Simulation: a battery that charges from a notional solar input and
 *     discharges under load, so the EPS FDIR/mode logic sees real dynamics. */
static void simulate(ina219_sample_t *out)
{
    static float soc_v = 4.0f; /* pretend battery terminal voltage */
    double t = (double) esp_timer_get_time() / 1e6;
    /* Solar input follows a slow sinusoid (orbit day/night) plus noise. */
    float solar = 0.5f + 0.5f * sinf((float) t / 30.0f);
    float load_ma = 120.0f + 40.0f * sinf((float) t / 7.0f);
    /* Integrate a crude charge balance. */
    soc_v += (solar * 0.02f - 0.008f);
    if (soc_v > 4.2f) soc_v = 4.2f;
    if (soc_v < 3.0f) soc_v = 3.0f;
    out->bus_voltage_v   = soc_v;
    out->shunt_current_ma = load_ma * (solar > 0.6f ? -1.0f : 1.0f);
    out->power_mw        = fabsf(out->bus_voltage_v * out->shunt_current_ma);
}

obc_err_t ina219_read(ina219_sample_t *out)
{
    if (out == NULL) {
        return OBC_ERR_INVALID_ARG;
    }
    if (!s_present) {
        simulate(out);
        return OBC_OK;
    }
    uint16_t raw_bus = 0, raw_cur = 0, raw_pwr = 0;
    obc_err_t rc = hal_i2c_read_u16_be(BSP_I2C_ADDR_INA219, REG_BUS, &raw_bus);
    rc |= hal_i2c_read_u16_be(BSP_I2C_ADDR_INA219, REG_CURRENT, &raw_cur);
    rc |= hal_i2c_read_u16_be(BSP_I2C_ADDR_INA219, REG_POWER, &raw_pwr);
    if (rc != OBC_OK) {
        return OBC_ERR_FAIL;
    }
    /* Bus voltage register: bits [15:3], LSB = 4 mV. */
    out->bus_voltage_v   = (float) (raw_bus >> 3) * 0.004f;
    out->shunt_current_ma = (int16_t) raw_cur * CURRENT_LSB_MA;
    out->power_mw        = raw_pwr * POWER_LSB_MW;
    return OBC_OK;
}
