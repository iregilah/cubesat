#include "obc_state.h"
#include "telecommand.h"
#include "bsp.h"

#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static const char *TAG = "state";

#define MODE_NAME(m) (s_mode_names[(m) < OBC_MODE_COUNT ? (m) : 0])
static const char *s_mode_names[] = {"BOOT", "SAFE", "NOMINAL", "PAYLOAD", "FAULT"};

/* --- shared objects --- */
static SemaphoreHandle_t  s_mtx;        /* guards the blackboard below */
static EventGroupHandle_t s_events;
static QueueHandle_t      s_cmd_q;
static QueueHandle_t      s_evt_q;

/* --- blackboard (only touched under s_mtx) --- */
static eps_telemetry_t     s_eps;
static adcs_telemetry_t    s_adcs;
static thermal_telemetry_t s_thermal;
static obc_mode_t          s_mode = OBC_MODE_BOOT;
static uint16_t            s_faults;

static inline void lock(void)   { xSemaphoreTake(s_mtx, portMAX_DELAY); }
static inline void unlock(void) { xSemaphoreGive(s_mtx); }

obc_err_t obc_state_init(void)
{
    s_mtx    = xSemaphoreCreateMutex();
    s_events = xEventGroupCreate();
    s_cmd_q  = xQueueCreate(8, sizeof(telecommand_t));
    s_evt_q  = xQueueCreate(16, sizeof(obc_event_t));
    if (!s_mtx || !s_events || !s_cmd_q || !s_evt_q) {
        return OBC_ERR_NO_MEM;
    }
    return OBC_OK;
}

EventGroupHandle_t obc_state_events(void) { return s_events; }

void obc_state_signal(EventBits_t bits) { xEventGroupSetBits(s_events, bits); }

bool obc_state_wait_all_ready(TickType_t timeout)
{
    EventBits_t got = xEventGroupWaitBits(s_events, EVT_ALL_READY,
                                          pdFALSE, pdTRUE, timeout);
    return (got & EVT_ALL_READY) == EVT_ALL_READY;
}

/* --- telemetry setters/getters --- */
void obc_state_set_eps(const eps_telemetry_t *t)
{ lock(); s_eps = *t; unlock(); }
void obc_state_set_adcs(const adcs_telemetry_t *t)
{ lock(); s_adcs = *t; unlock(); }
void obc_state_set_thermal(const thermal_telemetry_t *t)
{ lock(); s_thermal = *t; unlock(); }

void obc_state_get_eps(eps_telemetry_t *o)        { lock(); *o = s_eps; unlock(); }
void obc_state_get_adcs(adcs_telemetry_t *o)      { lock(); *o = s_adcs; unlock(); }
void obc_state_get_thermal(thermal_telemetry_t *o){ lock(); *o = s_thermal; unlock(); }

void obc_state_get_beacon(obc_beacon_t *out)
{
    lock();
    out->eps     = s_eps;
    out->adcs    = s_adcs;
    out->thermal = s_thermal;
    out->mode    = s_mode;
    out->fault_flags = s_faults;
    unlock();
    out->uptime_s    = (uint32_t) (esp_timer_get_time() / 1000000);
    out->mission_time_s = out->uptime_s; /* clock_svc refines this */
    out->boot_count  = bsp_boot_count();
}

/* --- mode & faults --- */
obc_mode_t obc_state_mode(void)
{
    lock();
    obc_mode_t m = s_mode;
    unlock();
    return m;
}

void obc_state_set_mode(obc_mode_t mode)
{
    lock();
    obc_mode_t prev = s_mode;
    s_mode = mode;
    unlock();
    if (prev != mode) {
        ESP_LOGI(TAG, "mode %s -> %s", MODE_NAME(prev), MODE_NAME(mode));
        bsp_set_mode_led(mode);
        obc_state_signal(EVT_MODE_CHANGED);
        obc_state_post_event(OBC_SS_CDH, "MODE %s->%s",
                             MODE_NAME(prev), MODE_NAME(mode));
    }
}

void obc_state_raise_fault(obc_fault_t f)
{
    lock();
    bool isnew = (s_faults & f) == 0;
    s_faults |= f;
    unlock();
    if (isnew) {
        obc_state_post_event(OBC_SS_CDH, "FAULT set 0x%02X", f);
    }
}

void obc_state_clear_fault(obc_fault_t f) { lock(); s_faults &= ~f; unlock(); }

uint16_t obc_state_faults(void)
{
    lock();
    uint16_t f = s_faults;
    unlock();
    return f;
}

/* --- queues --- */
QueueHandle_t obc_state_event_queue(void)   { return s_evt_q; }
QueueHandle_t obc_state_command_queue(void) { return s_cmd_q; }

void obc_state_post_event(obc_subsystem_t src, const char *fmt, ...)
{
    obc_event_t e = {
        .t_uptime_s = (uint32_t) (esp_timer_get_time() / 1000000),
        .source = src,
    };
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e.text, sizeof e.text, fmt, ap);
    va_end(ap);
    /* Non-blocking: if the GUI log is full, drop the oldest by overwriting. */
    if (xQueueSend(s_evt_q, &e, 0) != pdTRUE) {
        obc_event_t junk;
        xQueueReceive(s_evt_q, &junk, 0);
        xQueueSend(s_evt_q, &e, 0);
    }
}
