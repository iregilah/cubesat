/**
 * @file clock_svc.h
 * @brief Mission clock built on the DS3231 RTC (or esp_timer fallback).
 */
#ifndef CLOCK_SVC_H
#define CLOCK_SVC_H

#include <stdint.h>
#include <stddef.h>
#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

obc_err_t clock_svc_init(void);
/** Seconds since boot (monotonic). */
uint32_t  clock_svc_uptime_s(void);
/** Format current UTC as "YYYY-MM-DD hh:mm:ss" into @p buf (>=20 bytes). */
void      clock_svc_utc_str(char *buf, size_t buf_sz);

#ifdef __cplusplus
}
#endif

#endif /* CLOCK_SVC_H */
