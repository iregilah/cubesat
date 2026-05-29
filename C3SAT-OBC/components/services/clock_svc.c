#include "clock_svc.h"
#include "ds3231.h"

#include "esp_timer.h"
#include <stdio.h>
#include <time.h>

obc_err_t clock_svc_init(void)
{
    return ds3231_init();
}

uint32_t clock_svc_uptime_s(void)
{
    return (uint32_t) (esp_timer_get_time() / 1000000);
}

void clock_svc_utc_str(char *buf, size_t buf_sz)
{
    struct tm t;
    if (ds3231_get_time(&t) != OBC_OK) {
        snprintf(buf, buf_sz, "--------- --:--:--");
        return;
    }
    snprintf(buf, buf_sz, "%04d-%02d-%02d %02d:%02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
}
