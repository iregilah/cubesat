#include "fdir.h"
#include "obc_state.h"
#include "obc_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "fdir";

typedef struct {
    volatile uint32_t last_beat_ms;
    uint32_t          deadline_ms;
    bool              enabled;
    bool              tripped;
} hb_t;

static hb_t s_hb[OBC_SS_COUNT];

static uint32_t now_ms(void)
{
    return (uint32_t) (esp_timer_get_time() / 1000);
}

obc_err_t fdir_init(void)
{
    uint32_t t = now_ms();
    for (int i = 0; i < OBC_SS_COUNT; ++i) {
        s_hb[i].last_beat_ms = t;
        s_hb[i].deadline_ms  = 3000; /* generous default */
        s_hb[i].enabled      = false;
        s_hb[i].tripped      = false;
    }
    return OBC_OK;
}

void fdir_set_deadline(obc_subsystem_t ss, uint32_t deadline_ms)
{
    if (ss < OBC_SS_COUNT) {
        s_hb[ss].deadline_ms = deadline_ms;
        s_hb[ss].enabled = true;
        s_hb[ss].last_beat_ms = now_ms();
    }
}

void fdir_heartbeat(obc_subsystem_t ss)
{
    if (ss < OBC_SS_COUNT) {
        s_hb[ss].last_beat_ms = now_ms();
        if (s_hb[ss].tripped) {
            s_hb[ss].tripped = false;
            obc_state_post_event(ss, "heartbeat restored");
        }
    }
}

static void check_heartbeats(void)
{
    uint32_t t = now_ms();
    bool any_missed = false;
    for (int i = 0; i < OBC_SS_COUNT; ++i) {
        if (!s_hb[i].enabled || s_hb[i].tripped) {
            continue;
        }
        if (t - s_hb[i].last_beat_ms > s_hb[i].deadline_ms) {
            s_hb[i].tripped = true;
            any_missed = true;
            obc_state_post_event((obc_subsystem_t) i, "WATCHDOG missed");
            ESP_LOGE(TAG, "subsystem %d missed its heartbeat", i);
        }
    }
    if (any_missed) {
        obc_state_raise_fault(OBC_FAULT_WATCHDOG);
        /* Recovery: drop to SAFE so a wedged subsystem can't endanger the bird. */
        if (obc_state_mode() != OBC_MODE_SAFE) {
            obc_state_set_mode(OBC_MODE_SAFE);
        }
    }
}

static void log_health(void)
{
    /* Per-task stack headroom — the cheapest way to catch a stack about to
     * overflow before it corrupts a neighbour. */
    UBaseType_t n = uxTaskGetNumberOfTasks();
    ESP_LOGI(TAG, "tasks=%u free-heap=%u B min-free=%u B",
             (unsigned) n,
             (unsigned) esp_get_free_heap_size(),
             (unsigned) esp_get_minimum_free_heap_size());
}

static void fdir_task(void *arg)
{
    (void) arg;
    /* Subscribe ourselves to the hardware task watchdog: if FDIR ever blocks,
     * the WDT resets the OBC — the ultimate recovery action. */
    esp_task_wdt_add(NULL);

    uint32_t ticks = 0;
    for (;;) {
        esp_task_wdt_reset();
        check_heartbeats();
        if (++ticks % 10 == 0) {     /* every ~10 s */
            log_health();
        }
        vTaskDelay(pdMS_TO_TICKS(PERIOD_FDIR_MS));
    }
}

obc_err_t fdir_start(void)
{
    if (xTaskCreate(fdir_task, "fdir", STACK_FDIR, NULL, PRIO_FDIR, NULL) != pdPASS) {
        return OBC_ERR_NO_MEM;
    }
    return OBC_OK;
}
