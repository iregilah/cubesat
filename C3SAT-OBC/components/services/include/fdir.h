/**
 * @file fdir.h
 * @brief Fault Detection, Isolation & Recovery — the spacecraft safety net.
 *
 * Runs at the highest application priority. Two jobs:
 *   1. Software heartbeat watchdog: every subsystem task calls fdir_heartbeat()
 *      each loop. If one stops checking in within its deadline, FDIR latches a
 *      watchdog fault and commands SAFE mode. FDIR itself is subscribed to the
 *      ESP-IDF hardware Task Watchdog, so a hung FDIR reboots the OBC.
 *   2. Health telemetry: periodically logs per-task stack high-water marks and
 *      CPU usage so the design can be tuned with real evidence.
 */
#ifndef FDIR_H
#define FDIR_H

#include "obc_types.h"
#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

obc_err_t fdir_init(void);
obc_err_t fdir_start(void);
/** A monitored subsystem reports liveness (call once per loop iteration). */
void      fdir_heartbeat(obc_subsystem_t ss);
/** Set the max allowed gap between heartbeats for a subsystem (ms). */
void      fdir_set_deadline(obc_subsystem_t ss, uint32_t deadline_ms);

#ifdef __cplusplus
}
#endif

#endif /* FDIR_H */
