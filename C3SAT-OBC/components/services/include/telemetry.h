/**
 * @file telemetry.h
 * @brief Telemetry downlink + the log queue feeding mass memory.
 *
 * Two responsibilities:
 *   1. Frame a beacon snapshot and transmit it over the UART "radio" link
 *      (the downlink counterpart of telecommand's uplink).
 *   2. Own the queue that decouples the fast subsystem tasks from the slower
 *      storage task: producers drop beacon records in, storage drains them.
 */
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "obc_types.h"
#include "obc_errors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

obc_err_t     telemetry_init(void);
/** Encode + transmit a beacon on the downlink. Also enqueues it for logging. */
obc_err_t     telemetry_downlink(const obc_beacon_t *beacon);
/** Queue of obc_beacon_t records awaiting persistence (drained by storage). */
QueueHandle_t telemetry_log_queue(void);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_H */
