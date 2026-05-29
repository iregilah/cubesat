/**
 * @file ds3231.h
 * @brief DS3231 RTC — provides the mission clock / UTC timestamps.
 *
 * A spacecraft keeps a monotonic mission clock independent of reboots. If no
 * RTC is fitted, the driver falls back to the MCU's esp_timer so timestamps
 * still advance (just not across power cycles).
 */
#ifndef DS3231_H
#define DS3231_H

#include <stdbool.h>
#include <time.h>
#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

obc_err_t ds3231_init(void);
bool      ds3231_present(void);

/** Read wall-clock time into a struct tm (UTC). */
obc_err_t ds3231_get_time(struct tm *out);
/** Set the RTC wall-clock time. */
obc_err_t ds3231_set_time(const struct tm *in);
/** Read the RTC's internal temperature sensor [°C]. */
obc_err_t ds3231_get_temp(float *celsius);

#ifdef __cplusplus
}
#endif

#endif /* DS3231_H */
