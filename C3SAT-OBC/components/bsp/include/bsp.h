/**
 * @file bsp.h
 * @brief Board bring-up: NVS, status LED, boot counter.
 */
#ifndef BSP_H
#define BSP_H

#include <stdint.h>
#include "obc_errors.h"
#include "obc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise low-level board facilities.
 *
 * Brings up NVS (non-volatile storage, used as our "config EEPROM"), the
 * status RGB LED, and increments+returns the persistent boot counter. Must be
 * called once from app_main before any task is created.
 */
obc_err_t bsp_init(void);

/** @return Number of times the firmware has booted (persisted in NVS). */
uint32_t bsp_boot_count(void);

/** Set the status LED colour to reflect the current satellite mode. */
void bsp_set_mode_led(obc_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* BSP_H */
