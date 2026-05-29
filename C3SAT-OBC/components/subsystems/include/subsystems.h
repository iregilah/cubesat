/**
 * @file subsystems.h
 * @brief Entry points for the four spacecraft subsystem tasks.
 *
 * Each subsystem owns one FreeRTOS task that:
 *   - samples its sensor(s) at a fixed cadence (vTaskDelayUntil),
 *   - runs its local control/autonomy logic,
 *   - publishes telemetry to the shared blackboard,
 *   - reports a heartbeat to FDIR.
 *
 * They never talk to each other directly — all coupling goes through
 * obc_state, which keeps the concurrency model simple to reason about.
 */
#ifndef SUBSYSTEMS_H
#define SUBSYSTEMS_H

#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

obc_err_t eps_start(void);      /**< Electrical Power System task. */
obc_err_t adcs_start(void);     /**< Attitude Determination & Control task. */
obc_err_t thermal_start(void);  /**< Thermal / housekeeping task. */
obc_err_t cdh_start(void);      /**< Command & Data Handling task. */

/** Ground override of the survival heater: -1 autonomous, 0 off, 1 on. */
void      thermal_set_heater_override(int on);

#ifdef __cplusplus
}
#endif

#endif /* SUBSYSTEMS_H */
