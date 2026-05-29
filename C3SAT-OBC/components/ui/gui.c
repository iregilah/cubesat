#include "gui.h"
#include "gfx.h"
#include "obc_state.h"
#include "obc_config.h"
#include "clock_svc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *s_mode_names[] = {"BOOT", "SAFE", "NOMINAL", "PAYLOAD", "FAULT"};
static const uint16_t s_mode_colors[] = {
    GFX_WHITE, GFX_BLUE, GFX_GREEN, GFX_CYAN, GFX_RED
};

/* ---- panel geometry (320x240 landscape) ----*/
#define HDR_H    20
#define COL_L_X  4
#define COL_R_X  164
#define COL_W    152
#define LOG_LINES 5

typedef struct {
    uint32_t        t;
    obc_subsystem_t src;
    char            text[48];
} log_line_t;

static log_line_t s_log[LOG_LINES];
static int        s_log_count;

static const char *ss_name(obc_subsystem_t s)
{
    static const char *n[] = {"CDH", "EPS", "ADCS", "THRM", "PL"};
    return (s < OBC_SS_COUNT) ? n[s] : "?";
}

static void panel(int x, int y, int w, int h, const char *title)
{
    gfx_rect(x, y, w, h, GFX_DKGREY);
    gfx_fill_rect(x + 1, y + 1, w - 2, 10, GFX_NAVY);
    gfx_text(x + 4, y + 2, title, GFX_CYAN, GFX_NAVY, 1);
}

static void draw_static(void)
{
    gfx_clear(GFX_BLACK);
    /* Header. */
    gfx_fill_rect(0, 0, 320, HDR_H, GFX_DKGREY);
    gfx_text(4, 3, "C3SAT-OBC", GFX_YELLOW, GFX_DKGREY, 2);

    panel(COL_L_X, 36, COL_W, 64, "EPS  POWER");
    panel(COL_R_X, 36, COL_W, 64, "ADCS ATTITUDE");
    panel(COL_L_X, 104, COL_W, 48, "THERMAL");
    panel(COL_R_X, 104, COL_W, 48, "FAULTS");
    panel(COL_L_X, 156, 312, 80, "EVENT LOG");
}

static void draw_mode_banner(obc_mode_t mode)
{
    uint16_t c = s_mode_colors[mode < OBC_MODE_COUNT ? mode : 0];
    gfx_fill_rect(196, 2, 120, 16, GFX_DKGREY);
    gfx_printf(200, 3, c, GFX_DKGREY, 2, "%-7s", s_mode_names[mode]);
}

static void draw_eps(const eps_telemetry_t *e)
{
    int x = COL_L_X + 6, y = 50;
    gfx_printf(x, y,      GFX_WHITE, GFX_BLACK, 1, "VBUS %5.2f V", e->bus_voltage_v);
    gfx_printf(x, y + 12, GFX_WHITE, GFX_BLACK, 1, "IBUS %5.0f mA", e->bus_current_ma);
    gfx_printf(x, y + 24, e->charging ? GFX_GREEN : GFX_ORANGE, GFX_BLACK, 1,
               "%-8s SHED%d", e->charging ? "CHARGING" : "DISCHRG",
               e->load_shed_level);
    uint16_t soc_col = e->battery_soc_pct < 25.0f ? GFX_RED :
                       e->battery_soc_pct < 50.0f ? GFX_ORANGE : GFX_GREEN;
    gfx_bar(x, y + 38, 120, 8, e->battery_soc_pct / 100.0f, soc_col, GFX_BLACK);
    gfx_printf(x + 124, y + 38, GFX_WHITE, GFX_BLACK, 1, "%3.0f", e->battery_soc_pct);
}

static void draw_adcs(const adcs_telemetry_t *a)
{
    int x = COL_R_X + 6, y = 50;
    gfx_printf(x, y,      GFX_WHITE, GFX_BLACK, 1, "RATE %5.1f dps", a->rate_rms_dps);
    gfx_printf(x, y + 12, GFX_WHITE, GFX_BLACK, 1, "RPY %4.0f %4.0f %4.0f",
               a->euler_deg[0], a->euler_deg[1], a->euler_deg[2]);
    gfx_printf(x, y + 24, GFX_GREY, GFX_BLACK, 1, "GZ  %6.1f", a->gyro_dps[2]);
    gfx_printf(x, y + 38, a->detumbled ? GFX_GREEN : GFX_RED, GFX_BLACK, 1,
               "%-10s", a->detumbled ? "DETUMBLED" : "TUMBLING!");
}

static void draw_thermal(const thermal_telemetry_t *t)
{
    int x = COL_L_X + 6, y = 118;
    uint16_t tc = t->temperature_c > THERMAL_HOT_C ? GFX_RED :
                  t->temperature_c < THERMAL_COLD_C ? GFX_CYAN : GFX_WHITE;
    gfx_printf(x, y,      tc, GFX_BLACK, 1, "T %5.1f C", t->temperature_c);
    gfx_printf(x, y + 12, GFX_GREY, GFX_BLACK, 1, "P %6.1f hPa", t->pressure_hpa);
    gfx_printf(x, y + 24, t->heater_on ? GFX_ORANGE : GFX_GREY, GFX_BLACK, 1,
               "HEATER %-3s", t->heater_on ? "ON" : "OFF");
}

static void draw_faults(uint16_t faults)
{
    static const char *names[8] = {
        "EPS-UV", "EPS-OC", "T-HOT", "T-COLD",
        "TUMBLE", "SENSOR", "STORE", "WDOG"
    };
    int x = COL_R_X + 6, y = 118;
    if (faults == 0) {
        gfx_printf(x, y, GFX_GREEN, GFX_BLACK, 1, "ALL NOMINAL ");
        gfx_fill_rect(x, y + 12, COL_W - 14, 24, GFX_BLACK);
        return;
    }
    int col = 0, row = 0;
    for (int i = 0; i < 8; ++i) {
        uint16_t c = (faults & (1u << i)) ? GFX_RED : GFX_DKGREY;
        gfx_printf(x + col * 72, y + row * 12, c, GFX_BLACK, 1, "%-6s", names[i]);
        if (++col == 2) { col = 0; row++; }
    }
}

static void drain_events(void)
{
    QueueHandle_t q = obc_state_event_queue();
    obc_event_t e;
    bool changed = false;
    while (xQueueReceive(q, &e, 0) == pdTRUE) {
        /* Shift the ring up by one and append. */
        if (s_log_count < LOG_LINES) {
            s_log_count++;
        } else {
            memmove(&s_log[0], &s_log[1], sizeof(log_line_t) * (LOG_LINES - 1));
        }
        int idx = s_log_count - 1;
        s_log[idx].t = e.t_uptime_s;
        s_log[idx].src = e.source;
        strncpy(s_log[idx].text, e.text, sizeof s_log[idx].text - 1);
        s_log[idx].text[sizeof s_log[idx].text - 1] = '\0';
        changed = true;
    }
    if (!changed) {
        return;
    }
    int x = COL_L_X + 4, y = 168;
    for (int i = 0; i < LOG_LINES; ++i) {
        gfx_fill_rect(x, y + i * 13, 312 - 8, 9, GFX_BLACK);
        if (i < s_log_count) {
            gfx_printf(x, y + i * 13, GFX_GREEN, GFX_BLACK, 1, "%5lu %-4s %s",
                       (unsigned long) s_log[i].t, ss_name(s_log[i].src),
                       s_log[i].text);
        }
    }
}

static void gui_task(void *arg)
{
    (void) arg;
    gfx_init();
    draw_static();
    obc_state_signal(EVT_GUI_READY);

    obc_mode_t shown_mode = (obc_mode_t) 0xFF;
    TickType_t next = xTaskGetTickCount();

    for (;;) {
        /* Redraw the mode banner only when the mode actually changed. */
        EventBits_t bits = xEventGroupClearBits(obc_state_events(), EVT_MODE_CHANGED);
        obc_mode_t mode = obc_state_mode();
        if ((bits & EVT_MODE_CHANGED) || mode != shown_mode) {
            draw_mode_banner(mode);
            shown_mode = mode;
        }

        char ts[24];
        clock_svc_utc_str(ts, sizeof ts);
        gfx_printf(4, 24, GFX_GREY, GFX_BLACK, 1, "UP %lus  UTC %s",
                   (unsigned long) clock_svc_uptime_s(), ts);

        eps_telemetry_t e;     obc_state_get_eps(&e);
        adcs_telemetry_t a;    obc_state_get_adcs(&a);
        thermal_telemetry_t t; obc_state_get_thermal(&t);
        draw_eps(&e);
        draw_adcs(&a);
        draw_thermal(&t);
        draw_faults(obc_state_faults());
        drain_events();

        vTaskDelayUntil(&next, pdMS_TO_TICKS(PERIOD_GUI_MS));
    }
}

obc_err_t gui_start(void)
{
    if (xTaskCreate(gui_task, "gui", STACK_GUI, NULL, PRIO_GUI, NULL) != pdPASS) {
        return OBC_ERR_NO_MEM;
    }
    return OBC_OK;
}
