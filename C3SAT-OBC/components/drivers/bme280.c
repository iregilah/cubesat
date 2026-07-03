#include "bme280.h"
#include "hal_i2c.h"
#include "bsp_pins.h"

#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "bme280";

#define REG_ID        0xD0
#define REG_RESET     0xE0
#define REG_CTRL_HUM  0xF2
#define REG_CTRL_MEAS 0xF4
#define REG_CONFIG    0xF5
#define REG_PRESS_MSB 0xF7   /* burst: press(3) temp(3) hum(2) */
#define REG_CALIB00   0x88   /* T/P calibration, 26 bytes */
#define REG_CALIB26   0xE1   /* H calibration, 7 bytes */
#define CHIP_ID_BME280 0x60  /* BME280: temperature + pressure + humidity. */
#define CHIP_ID_BMP280 0x58  /* BMP280: temperature + pressure only.        */

static bool s_present;
static bool s_has_humidity;  /* false on a BMP280 (no humidity channel). */

/* Bosch compensation parameters (read from the device's NVM). */
static uint16_t dig_T1; static int16_t dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t  dig_H1, dig_H3; static int16_t dig_H2, dig_H4, dig_H5;
static int8_t   dig_H6;
static int32_t  t_fine;

static obc_err_t read_calibration(void)
{
    uint8_t c[26];
    if (hal_i2c_read_reg(BSP_I2C_ADDR_BME280, REG_CALIB00, c, sizeof c) != OBC_OK) {
        return OBC_ERR_FAIL;
    }
    dig_T1 = (c[1] << 8) | c[0];   dig_T2 = (c[3] << 8) | c[2];
    dig_T3 = (c[5] << 8) | c[4];   dig_P1 = (c[7] << 8) | c[6];
    dig_P2 = (c[9] << 8) | c[8];   dig_P3 = (c[11] << 8) | c[10];
    dig_P4 = (c[13] << 8) | c[12]; dig_P5 = (c[15] << 8) | c[14];
    dig_P6 = (c[17] << 8) | c[16]; dig_P7 = (c[19] << 8) | c[18];
    dig_P8 = (c[21] << 8) | c[20]; dig_P9 = (c[23] << 8) | c[22];
    dig_H1 = c[25];

    if (!s_has_humidity) {
        return OBC_OK;   /* BMP280 has no humidity NVM block. */
    }

    uint8_t h[7];
    if (hal_i2c_read_reg(BSP_I2C_ADDR_BME280, REG_CALIB26, h, sizeof h) != OBC_OK) {
        return OBC_ERR_FAIL;
    }
    dig_H2 = (h[1] << 8) | h[0];
    dig_H3 = h[2];
    dig_H4 = (h[3] << 4) | (h[4] & 0x0F);
    dig_H5 = (h[5] << 4) | (h[4] >> 4);
    dig_H6 = (int8_t) h[6];
    return OBC_OK;
}

obc_err_t bme280_init(void)
{
    uint8_t id = 0;
    if (hal_i2c_probe(BSP_I2C_ADDR_BME280) != OBC_OK ||
        hal_i2c_read_u8(BSP_I2C_ADDR_BME280, REG_ID, &id) != OBC_OK ||
        (id != CHIP_ID_BME280 && id != CHIP_ID_BMP280)) {
        s_present = false;
        ESP_LOGW(TAG, "not found -> simulated thermal");
        return OBC_OK;
    }
    /* Same register map for T/P; the BMP280 simply lacks the humidity channel. */
    s_has_humidity = (id == CHIP_ID_BME280);

    if (read_calibration() != OBC_OK) {
        s_present = false;
        return OBC_OK;
    }
    if (s_has_humidity) {
        hal_i2c_write_u8(BSP_I2C_ADDR_BME280, REG_CTRL_HUM, 0x01); /* hum x1 */
    }
    hal_i2c_write_u8(BSP_I2C_ADDR_BME280, REG_CONFIG, 0xA0);    /* tstby 1s */
    hal_i2c_write_u8(BSP_I2C_ADDR_BME280, REG_CTRL_MEAS, 0x27); /* T x1 P x1 normal */
    s_present = true;
    ESP_LOGI(TAG, "%s online", s_has_humidity ? "BME280" : "BMP280");
    return OBC_OK;
}

bool bme280_present(void) { return s_present; }

/* Bosch fixed-point compensation (datasheet §8.1). */
static float compensate_T(int32_t adc_T)
{
    int32_t v1 = ((((adc_T >> 3) - ((int32_t) dig_T1 << 1))) * dig_T2) >> 11;
    int32_t v2 = (((((adc_T >> 4) - dig_T1) * ((adc_T >> 4) - dig_T1)) >> 12) * dig_T3) >> 14;
    t_fine = v1 + v2;
    return ((t_fine * 5 + 128) >> 8) / 100.0f;
}

static float compensate_P(int32_t adc_P)
{
    int64_t v1 = (int64_t) t_fine - 128000;
    int64_t v2 = v1 * v1 * dig_P6;
    v2 += (v1 * dig_P5) << 17;
    v2 += ((int64_t) dig_P4) << 35;
    v1 = ((v1 * v1 * dig_P3) >> 8) + ((v1 * dig_P2) << 12);
    v1 = (((((int64_t) 1) << 47) + v1) * dig_P1) >> 33;
    if (v1 == 0) {
        return 0.0f;
    }
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - v2) * 3125) / v1;
    v1 = (dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    v2 = (dig_P8 * p) >> 19;
    p = ((p + v1 + v2) >> 8) + (((int64_t) dig_P7) << 4);
    return (float) p / 256.0f / 100.0f; /* Pa -> hPa */
}

static float compensate_H(int32_t adc_H)
{
    int32_t v = t_fine - 76800;
    v = ((((adc_H << 14) - (((int32_t) dig_H4) << 20) - (dig_H5 * v)) + 16384) >> 15) *
        (((((((v * dig_H6) >> 10) * (((v * dig_H3) >> 11) + 32768)) >> 10) + 2097152) *
          dig_H2 + 8192) >> 14);
    v -= (((((v >> 15) * (v >> 15)) >> 7) * dig_H1) >> 4);
    if (v < 0) v = 0;
    if (v > 419430400) v = 419430400;
    return (v >> 12) / 1024.0f;
}

static void simulate(bme280_sample_t *out)
{
    double t = (double) esp_timer_get_time() / 1e6;
    /* Orbital thermal swing between roughly -15 and +50 °C. */
    out->temperature_c = 17.0f + 33.0f * sinf((float) t / 45.0f);
    out->pressure_hpa  = 1013.0f + 5.0f * sinf((float) t / 60.0f);
    out->humidity_pct  = 40.0f + 10.0f * sinf((float) t / 25.0f);
}

obc_err_t bme280_read(bme280_sample_t *out)
{
    if (out == NULL) {
        return OBC_ERR_INVALID_ARG;
    }
    if (!s_present) {
        simulate(out);
        return OBC_OK;
    }
    /* BME280 bursts press(3) temp(3) hum(2); a BMP280 stops after temp. */
    uint8_t d[8];
    size_t n = s_has_humidity ? 8 : 6;
    if (hal_i2c_read_reg(BSP_I2C_ADDR_BME280, REG_PRESS_MSB, d, n) != OBC_OK) {
        return OBC_ERR_FAIL;
    }
    int32_t adc_P = (d[0] << 12) | (d[1] << 4) | (d[2] >> 4);
    int32_t adc_T = (d[3] << 12) | (d[4] << 4) | (d[5] >> 4);
    out->temperature_c = compensate_T(adc_T); /* sets t_fine first */
    out->pressure_hpa  = compensate_P(adc_P);
    if (s_has_humidity) {
        int32_t adc_H = (d[6] << 8) | d[7];
        out->humidity_pct = compensate_H(adc_H);
    } else {
        out->humidity_pct = 0.0f;   /* BMP280: no humidity sensor. */
    }
    return OBC_OK;
}
