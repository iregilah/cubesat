#include "gui.h"
#include "gfx.h"
#include "obc_state.h"
#include "obc_config.h"
#include "clock_svc.h"
#include "touch.h"
#include "bsp.h"
#include "telecommand.h"
#include "ili9341.h"
#include "ina219.h"
#include "mpu6050.h"
#include "bme280.h"
#include "ds3231.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <stdio.h>
#include <string.h>

static const char *s_mode_names[] = {"BOOT", "SAFE", "NOMINAL", "PAYLOAD", "FAULT"};
static const uint16_t s_mode_colors[] = {
    GFX_WHITE, GFX_BLUE, GFX_GREEN, GFX_CYAN, GFX_RED
};

/* ---- screen views (the touch menu state machine) ------------------------ */
typedef enum {
    VIEW_DASHBOARD = 0,
    VIEW_MENU,
    VIEW_SENSORS,
    VIEW_MODES,
    VIEW_LOG,
    VIEW_ABOUT,
} view_t;

/* ---- shared geometry (320x240 landscape) -------------------------------- */
#define HDR_H     20
#define BTN_X     270          /* header MENU/BACK button */
#define BTN_W     50
#define COL_L_X   4
#define COL_R_X   164
#define COL_W     152

#define MENU_ROW_H  34
#define MENU_Y0     40
#define MENU_ITEMS  5

/* ---- event log ring (dashboard shows the tail, LOG view shows more) ------ */
#define LOG_MAX        16
#define LOG_DASH_LINES 5
#define LOG_VIEW_LINES 14

typedef struct {
    uint32_t        t;
    obc_subsystem_t src;
    char            text[48];
} log_line_t;

static log_line_t s_log[LOG_MAX];
static int        s_log_count;
static bool       s_log_dirty;

static view_t     s_view = VIEW_DASHBOARD;
static bool       s_redraw_static = true;   /* force a full repaint of s_view */
static bool       s_backlight_on = true;

static const char *ss_name(obc_subsystem_t s)
{
    static const char *n[] = {"CDH", "EPS", "ADCS", "THRM", "PL"};
    return (s < OBC_SS_COUNT) ? n[s] : "?";
}

/* ---- small primitives ---------------------------------------------------- */
static bool in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void button(int x, int y, int w, int h, const char *label,
                   uint16_t fg, uint16_t bg)
{
    gfx_fill_rect(x, y, w, h, bg);
    gfx_rect(x, y, w, h, GFX_GREY);
    int tw = (int) strlen(label) * gfx_char_w(2);
    int tx = x + (w - tw) / 2;
    int ty = y + (h - gfx_char_h(2)) / 2;
    gfx_text(tx, ty, label, fg, bg, 2);
}

static void panel(int x, int y, int w, int h, const char *title)
{
    gfx_rect(x, y, w, h, GFX_DKGREY);
    gfx_fill_rect(x + 1, y + 1, w - 2, 10, GFX_NAVY);
    gfx_text(x + 4, y + 2, title, GFX_CYAN, GFX_NAVY, 1);
}

/* Header shared by every view: title on the left, MENU/BACK button on right. */
static void draw_header(const char *title, bool dashboard)
{
    gfx_fill_rect(0, 0, 320, HDR_H, GFX_DKGREY);
    gfx_text(4, 3, title, GFX_YELLOW, GFX_DKGREY, 2);
    button(BTN_X, 0, BTN_W, HDR_H, dashboard ? "MENU" : "BACK", GFX_WHITE, GFX_NAVY);
}

/* ---- dashboard panels (unchanged content) -------------------------------- */
static void draw_mode_banner(obc_mode_t mode)
{
    uint16_t c = s_mode_colors[mode < OBC_MODE_COUNT ? mode : 0];
    gfx_fill_rect(140, 2, 120, 16, GFX_DKGREY);
    gfx_printf(144, 3, c, GFX_DKGREY, 2, "%-7s", s_mode_names[mode]);
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

static void draw_dashboard_static(void)
{
    gfx_clear(GFX_BLACK);
    draw_header("C3SAT-OBC", true);
    panel(COL_L_X, 36, COL_W, 64, "EPS  POWER");
    panel(COL_R_X, 36, COL_W, 64, "ADCS ATTITUDE");
    panel(COL_L_X, 104, COL_W, 48, "THERMAL");
    panel(COL_R_X, 104, COL_W, 48, "FAULTS");
    panel(COL_L_X, 156, 312, 80, "EVENT LOG");
}

/* ---- event log ---------------------------------------------------------- */
static void drain_events(void)
{
    QueueHandle_t q = obc_state_event_queue();
    obc_event_t e;
    while (xQueueReceive(q, &e, 0) == pdTRUE) {
        if (s_log_count < LOG_MAX) {
            s_log_count++;
        } else {
            memmove(&s_log[0], &s_log[1], sizeof(log_line_t) * (LOG_MAX - 1));
        }
        int idx = s_log_count - 1;
        s_log[idx].t = e.t_uptime_s;
        s_log[idx].src = e.source;
        strncpy(s_log[idx].text, e.text, sizeof s_log[idx].text - 1);
        s_log[idx].text[sizeof s_log[idx].text - 1] = '\0';
        s_log_dirty = true;
    }
}

/* Render the newest @p lines log entries starting at pixel (x,y). */
static void draw_log_tail(int x, int y, int lines, int row_px, int clr_w)
{
    int first = s_log_count > lines ? s_log_count - lines : 0;
    for (int i = 0; i < lines; ++i) {
        gfx_fill_rect(x, y + i * row_px, clr_w, 9, GFX_BLACK);
        int idx = first + i;
        if (idx < s_log_count) {
            gfx_printf(x, y + i * row_px, GFX_GREEN, GFX_BLACK, 1, "%5lu %-4s %s",
                       (unsigned long) s_log[idx].t, ss_name(s_log[idx].src),
                       s_log[idx].text);
        }
    }
}

/* ---- MENU view ---------------------------------------------------------- */
static void menu_label(int i, char *out, size_t n)
{
    switch (i) {
        case 0: snprintf(out, n, "SENSORS");    break;
        case 1: snprintf(out, n, "MODE CTRL");  break;
        case 2: snprintf(out, n, "EVENT LOG");  break;
        case 3: snprintf(out, n, "ABOUT");      break;
        case 4: snprintf(out, n, "BACKLIGHT %s", s_backlight_on ? "ON" : "OFF"); break;
        default: out[0] = '\0'; break;
    }
}

static void draw_menu_static(void)
{
    gfx_clear(GFX_BLACK);
    draw_header("MENU", false);
    for (int i = 0; i < MENU_ITEMS; ++i) {
        char lbl[24];
        menu_label(i, lbl, sizeof lbl);
        button(8, MENU_Y0 + i * MENU_ROW_H, 304, MENU_ROW_H - 4, lbl,
               GFX_WHITE, GFX_NAVY);
    }
}

/* ---- SENSORS view ------------------------------------------------------- */
static void sensor_line(int y, const char *name, bool online)
{
    gfx_printf(10, y, GFX_WHITE, GFX_BLACK, 1, "%-9s", name);
    gfx_printf(10 + 9 * gfx_char_w(1), y, online ? GFX_GREEN : GFX_ORANGE,
               GFX_BLACK, 1, "%-9s", online ? "ONLINE" : "SIMULATED");
}

static void draw_sensors_static(void)
{
    gfx_clear(GFX_BLACK);
    draw_header("SENSORS", false);
    panel(4, 28, 312, 74, "BUS STATUS (I2C @0x40/69/76/68)");
    panel(4, 108, 312, 124, "LIVE VALUES");
}

static void draw_sensors_dynamic(void)
{
    sensor_line(44, "INA219",  ina219_present());
    sensor_line(56, "MPU6050", mpu6050_present());
    sensor_line(68, "BMx280",  bme280_present());
    sensor_line(80, "DS3231",  ds3231_present());

    eps_telemetry_t e;     obc_state_get_eps(&e);
    adcs_telemetry_t a;    obc_state_get_adcs(&a);
    thermal_telemetry_t t; obc_state_get_thermal(&t);
    int y = 124;
    gfx_printf(10, y,      GFX_WHITE, GFX_BLACK, 1, "EPS  %5.2f V  %5.0f mA  SoC %3.0f%%",
               e.bus_voltage_v, e.bus_current_ma, e.battery_soc_pct);
    gfx_printf(10, y + 16, GFX_WHITE, GFX_BLACK, 1, "ADCS rate %5.1f dps  %-9s",
               a.rate_rms_dps, a.detumbled ? "DETUMBLED" : "TUMBLING");
    gfx_printf(10, y + 32, GFX_WHITE, GFX_BLACK, 1, "     RPY %4.0f %4.0f %4.0f",
               a.euler_deg[0], a.euler_deg[1], a.euler_deg[2]);
    gfx_printf(10, y + 48, GFX_WHITE, GFX_BLACK, 1, "THRM %5.1f C  %6.1f hPa",
               t.temperature_c, t.pressure_hpa);
    gfx_printf(10, y + 64, t.heater_on ? GFX_ORANGE : GFX_GREY, GFX_BLACK, 1,
               "     HEATER %-3s  MCU %4.1f C", t.heater_on ? "ON" : "OFF", t.mcu_temp_c);
    gfx_printf(10, y + 84, GFX_GREY, GFX_BLACK, 1, "(live from the blackboard)");
}

/* ---- MODE CTRL view ----------------------------------------------------- */
static const obc_mode_t s_mode_btn[3] = { OBC_MODE_SAFE, OBC_MODE_NOMINAL, OBC_MODE_PAYLOAD };

static void draw_modes_static(void)
{
    gfx_clear(GFX_BLACK);
    draw_header("MODE CTRL", false);
    gfx_text(10, 30, "REQUEST A MODE (FDIR MAY VETO):", GFX_GREY, GFX_BLACK, 1);
    for (int i = 0; i < 3; ++i) {
        button(8 + i * 102, 60, 96, 60, s_mode_names[s_mode_btn[i]],
               GFX_WHITE, GFX_NAVY);
    }
}

static void draw_modes_dynamic(void)
{
    obc_mode_t m = obc_state_mode();
    gfx_fill_rect(0, 140, 320, 20, GFX_BLACK);
    gfx_printf(10, 144, s_mode_colors[m < OBC_MODE_COUNT ? m : 0], GFX_BLACK, 2,
               "NOW: %-7s", s_mode_names[m]);
    /* Highlight the button matching the active mode. */
    for (int i = 0; i < 3; ++i) {
        uint16_t bg = (s_mode_btn[i] == m) ? GFX_DKGREY : GFX_NAVY;
        button(8 + i * 102, 60, 96, 60, s_mode_names[s_mode_btn[i]], GFX_WHITE, bg);
    }
}

static void request_mode(obc_mode_t mode)
{
    /* Route through the same command queue the ground link uses, so the CDH
     * task and mode-manager autonomy stay the single authority over mode. */
    telecommand_t cmd = {
        .apid = OBC_SS_CDH,
        .opcode = TC_OP_SET_MODE,
        .seq = 0,
        .len = 1,
    };
    cmd.payload[0] = (uint8_t) mode;
    xQueueSend(obc_state_command_queue(), &cmd, 0);
    obc_state_post_event(OBC_SS_CDH, "UI req mode %s", s_mode_names[mode]);
}

/* ---- LOG view ----------------------------------------------------------- */
static void draw_log_static(void)
{
    gfx_clear(GFX_BLACK);
    draw_header("EVENT LOG", false);
    s_log_dirty = true;   /* force the body to paint */
}

/* ---- ABOUT view --------------------------------------------------------- */
static void draw_about_static(void)
{
    gfx_clear(GFX_BLACK);
    draw_header("ABOUT", false);
    gfx_text(10, 34,  "C3SAT-OBC", GFX_YELLOW, GFX_BLACK, 2);
    gfx_text(10, 58,  "CubeSat On-Board Computer", GFX_WHITE, GFX_BLACK, 1);
    gfx_text(10, 70,  "FreeRTOS / ESP32-C6 reference", GFX_WHITE, GFX_BLACK, 1);
    gfx_text(10, 90,  "C3S embedded SW application", GFX_GREY, GFX_BLACK, 1);
    gfx_text(10, 102, "c3s.hu/positions", GFX_CYAN, GFX_BLACK, 1);
}

static void draw_about_dynamic(void)
{
    gfx_printf(10, 130, GFX_WHITE, GFX_BLACK, 1, "boot count : %lu",
               (unsigned long) bsp_boot_count());
    gfx_printf(10, 142, GFX_WHITE, GFX_BLACK, 1, "uptime     : %lu s",
               (unsigned long) clock_svc_uptime_s());
    gfx_printf(10, 154, GFX_WHITE, GFX_BLACK, 1, "free heap  : %u B",
               (unsigned) esp_get_free_heap_size());
    gfx_printf(10, 172, GFX_GREY, GFX_BLACK, 1, "tap BACK to return to the menu");
}

/* ---- touch dispatch ----------------------------------------------------- */
static void go(view_t v)
{
    s_view = v;
    s_redraw_static = true;
}

static void handle_tap(const touch_tap_t *tap)
{
    int x = tap->x, y = tap->y;

    /* Header button is present on every view. */
    if (in_rect(x, y, BTN_X, 0, BTN_W, HDR_H)) {
        go(s_view == VIEW_DASHBOARD ? VIEW_MENU :
           s_view == VIEW_MENU      ? VIEW_DASHBOARD : VIEW_MENU);
        return;
    }

    switch (s_view) {
        case VIEW_MENU:
            for (int i = 0; i < MENU_ITEMS; ++i) {
                if (in_rect(x, y, 8, MENU_Y0 + i * MENU_ROW_H, 304, MENU_ROW_H - 4)) {
                    switch (i) {
                        case 0: go(VIEW_SENSORS); break;
                        case 1: go(VIEW_MODES);   break;
                        case 2: go(VIEW_LOG);     break;
                        case 3: go(VIEW_ABOUT);   break;
                        case 4:
                            s_backlight_on = !s_backlight_on;
                            ili9341_backlight(s_backlight_on);
                            s_redraw_static = true; /* refresh the ON/OFF label */
                            break;
                    }
                    return;
                }
            }
            break;
        case VIEW_MODES:
            for (int i = 0; i < 3; ++i) {
                if (in_rect(x, y, 8 + i * 102, 60, 96, 60)) {
                    request_mode(s_mode_btn[i]);
                    return;
                }
            }
            break;
        default:
            break;
    }
}

/* ---- the task ----------------------------------------------------------- */
static void gui_task(void *arg)
{
    (void) arg;
    gfx_init();
    obc_state_signal(EVT_GUI_READY);

    QueueHandle_t tapq = touch_tap_queue();
    obc_mode_t shown_mode = (obc_mode_t) 0xFF;
    TickType_t next = xTaskGetTickCount();

    for (;;) {
        /* 1. input: drain taps (non-blocking) and act on them. */
        touch_tap_t tap;
        while (tapq && xQueueReceive(tapq, &tap, 0) == pdTRUE) {
            handle_tap(&tap);
        }

        /* 2. keep the event ring current regardless of the active view. */
        drain_events();

        /* 3. full repaint on a view change. */
        if (s_redraw_static) {
            switch (s_view) {
                case VIEW_DASHBOARD: draw_dashboard_static(); shown_mode = 0xFF; break;
                case VIEW_MENU:      draw_menu_static();    break;
                case VIEW_SENSORS:   draw_sensors_static(); break;
                case VIEW_MODES:     draw_modes_static();   break;
                case VIEW_LOG:       draw_log_static();     break;
                case VIEW_ABOUT:     draw_about_static();   break;
            }
            s_redraw_static = false;
        }

        /* 4. per-view live updates. */
        switch (s_view) {
            case VIEW_DASHBOARD: {
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
                if (s_log_dirty) {
                    draw_log_tail(COL_L_X + 4, 168, LOG_DASH_LINES, 13, 312 - 8);
                    s_log_dirty = false;
                }
                break;
            }
            case VIEW_SENSORS: draw_sensors_dynamic(); break;
            case VIEW_MODES:   draw_modes_dynamic();   break;
            case VIEW_ABOUT:   draw_about_dynamic();   break;
            case VIEW_LOG:
                if (s_log_dirty) {
                    draw_log_tail(6, 28, LOG_VIEW_LINES, 15, 320 - 12);
                    s_log_dirty = false;
                }
                break;
            default: break;
        }

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
