/**
 * @file gui.h
 * @brief Mission-control dashboard task on the ILI9341.
 *
 * This is the "extra parallel graphical event" running alongside the flight
 * software: a low-priority task that renders live telemetry from the shared
 * blackboard and an event log drained from the event queue. It demonstrates a
 * GUI cooperating with hard real-time tasks without ever blocking them.
 */
#ifndef GUI_H
#define GUI_H

#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

obc_err_t gui_start(void);

#ifdef __cplusplus
}
#endif

#endif /* GUI_H */
