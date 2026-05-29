#include "ds3231.h"
#include "hal_i2c.h"
#include "bsp_pins.h"

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ds3231";

#define REG_SECONDS  0x00
#define REG_TEMP_MSB 0x11

static bool s_present;

static uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

obc_err_t ds3231_init(void)
{
    s_present = (hal_i2c_probe(BSP_I2C_ADDR_DS3231) == OBC_OK);
    ESP_LOGI(TAG, s_present ? "DS3231 online" : "no RTC -> esp_timer clock");
    return OBC_OK;
}

bool ds3231_present(void) { return s_present; }

obc_err_t ds3231_get_time(struct tm *out)
{
    if (out == NULL) {
        return OBC_ERR_INVALID_ARG;
    }
    if (!s_present) {
        time_t now = (time_t) (esp_timer_get_time() / 1000000);
        gmtime_r(&now, out);
        return OBC_OK;
    }
    uint8_t r[7];
    if (hal_i2c_read_reg(BSP_I2C_ADDR_DS3231, REG_SECONDS, r, sizeof r) != OBC_OK) {
        return OBC_ERR_FAIL;
    }
    out->tm_sec  = bcd2dec(r[0] & 0x7F);
    out->tm_min  = bcd2dec(r[1] & 0x7F);
    out->tm_hour = bcd2dec(r[2] & 0x3F);
    out->tm_mday = bcd2dec(r[4] & 0x3F);
    out->tm_mon  = bcd2dec(r[5] & 0x1F) - 1;
    out->tm_year = bcd2dec(r[6]) + 100; /* years since 1900, assume 20xx */
    out->tm_isdst = 0;
    return OBC_OK;
}

obc_err_t ds3231_set_time(const struct tm *in)
{
    if (in == NULL) {
        return OBC_ERR_INVALID_ARG;
    }
    if (!s_present) {
        return OBC_ERR_NO_DEVICE;
    }
    uint8_t r[7] = {
        dec2bcd(in->tm_sec), dec2bcd(in->tm_min), dec2bcd(in->tm_hour),
        1, /* day-of-week, unused */
        dec2bcd(in->tm_mday), dec2bcd(in->tm_mon + 1),
        dec2bcd(in->tm_year % 100),
    };
    return hal_i2c_write_reg(BSP_I2C_ADDR_DS3231, REG_SECONDS, r, sizeof r);
}

obc_err_t ds3231_get_temp(float *celsius)
{
    if (celsius == NULL) {
        return OBC_ERR_INVALID_ARG;
    }
    if (!s_present) {
        *celsius = 25.0f;
        return OBC_OK;
    }
    uint8_t t[2];
    if (hal_i2c_read_reg(BSP_I2C_ADDR_DS3231, REG_TEMP_MSB, t, 2) != OBC_OK) {
        return OBC_ERR_FAIL;
    }
    *celsius = (int8_t) t[0] + ((t[1] >> 6) * 0.25f);
    return OBC_OK;
}
