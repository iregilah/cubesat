/**
 * @file obc_state.h
 * @brief Central inter-task communication hub (the "spacecraft blackboard").
 *
 * This module owns every shared FreeRTOS object so the wiring of the system is
 * visible in one place:
 *
 *   - A mutex-protected blackboard holding the latest telemetry from each
 *     subsystem. Producer tasks (EPS/ADCS/Thermal) publish; consumer tasks
 *     (CDH/GUI/Storage) read consistent snapshots.
 *   - An event group used both as a *startup barrier* (app_main waits until
 *     every subsystem signals "ready") and as a *change notification* channel
 *     (a bit is set whenever the satellite mode changes, so the GUI can redraw
 *     lazily instead of polling).
 *   - A command queue (telecommand task -> CDH) and an event queue
 *     (any task -> GUI event log).
 *
 * Keeping the synchronisation primitives behind a small typed API avoids the
 * classic flight-software bug of tasks grabbing the wrong mutex or forgetting
 * to give it back.
 */
#ifndef OBC_STATE_H
#define OBC_STATE_H

#include "obc_types.h"
#include "obc_errors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Event-group bits: subsystem readiness barrier + change notifications. */
#define EVT_CDH_READY      (1u << 0)
#define EVT_EPS_READY      (1u << 1)
#define EVT_ADCS_READY     (1u << 2)
#define EVT_THERMAL_READY  (1u << 3)
#define EVT_GUI_READY      (1u << 4)
#define EVT_ALL_READY      (EVT_CDH_READY | EVT_EPS_READY | EVT_ADCS_READY | \
                            EVT_THERMAL_READY | EVT_GUI_READY)
#define EVT_MODE_CHANGED   (1u << 8)

/** One line for the on-screen / downlinked event log. */
typedef struct {
    uint32_t        t_uptime_s;
    obc_subsystem_t source;
    char            text[48];
} obc_event_t;

/** Allocate all shared objects. Call once before creating any task. */
obc_err_t obc_state_init(void);

/* --- Event group ------------------------------------------------------- */
EventGroupHandle_t obc_state_events(void);
/** Set a readiness/notification bit. */
void obc_state_signal(EventBits_t bits);
/** Block until all EVT_*_READY bits are set or timeout elapses. */
bool obc_state_wait_all_ready(TickType_t timeout);

/* --- Telemetry blackboard (mutex-guarded copies) ----------------------- */
void obc_state_set_eps(const eps_telemetry_t *t);
void obc_state_set_adcs(const adcs_telemetry_t *t);
void obc_state_set_thermal(const thermal_telemetry_t *t);
void obc_state_get_eps(eps_telemetry_t *out);
void obc_state_get_adcs(adcs_telemetry_t *out);
void obc_state_get_thermal(thermal_telemetry_t *out);

/** Assemble a coherent whole-spacecraft snapshot for beacon/dashboard. */
void obc_state_get_beacon(obc_beacon_t *out);

/* --- Mode & faults ----------------------------------------------------- */
obc_mode_t obc_state_mode(void);
/** Set the mode; sets EVT_MODE_CHANGED and posts an event if it changed. */
void       obc_state_set_mode(obc_mode_t mode);
void       obc_state_raise_fault(obc_fault_t f);
void       obc_state_clear_fault(obc_fault_t f);
uint16_t   obc_state_faults(void);

/* --- Event log queue --------------------------------------------------- */
QueueHandle_t obc_state_event_queue(void);
/** Post a formatted event (safe to call from any task). */
void obc_state_post_event(obc_subsystem_t src, const char *fmt, ...);

/* --- Command queue ----------------------------------------------------- */
QueueHandle_t obc_state_command_queue(void);

#ifdef __cplusplus
}
#endif

#endif /* OBC_STATE_H */
