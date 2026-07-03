#include "touch.h"
#include "bsp_pins.h"
#include "ili9341.h"

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "touch";

#define TOUCH_TASK_PERIOD_MS 20   /* 50 Hz sampling */
#define TOUCH_STACK          3072
#define TOUCH_PRIO           3
#define ADC_MAX              4095  /* 12-bit oneshot */

static adc_oneshot_unit_handle_t s_adc;
static QueueHandle_t             s_tapq;

/* ------------------------------------------------------------------ helpers */

static inline void pin_out(int gpio, int level)
{
    gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(gpio, level);
}

static inline void pin_hiz(int gpio)
{
    gpio_set_direction(gpio, GPIO_MODE_INPUT);
    gpio_set_pull_mode(gpio, GPIO_FLOATING);
}

/* Input with an internal pull-down: an OPEN pad settles at ~0 (deterministic
 * "no touch"), while a real ~hundreds-of-ohm contact easily overrides it. */
static inline void pin_pd(int gpio)
{
    gpio_set_direction(gpio, GPIO_MODE_INPUT);
    gpio_set_pull_mode(gpio, GPIO_PULLDOWN_ONLY);
}

static int adc_get(adc_channel_t ch)
{
    int raw = 0;
    if (adc_oneshot_read(s_adc, ch, &raw) != ESP_OK) {
        return -1;
    }
    return raw;
}

/* Read position along the X plate: drive X+/X- as the gradient, sample Y+. */
static int read_x_raw(void)
{
    pin_hiz(BSP_PIN_TOUCH_YM);
    pin_hiz(BSP_PIN_TOUCH_YP);   /* sampled by ADC */
    pin_out(BSP_PIN_TOUCH_XP, 1);
    pin_out(BSP_PIN_TOUCH_XM, 0);
    esp_rom_delay_us(50);        /* let the plate settle */
    return adc_get(BSP_TOUCH_CH_YP);
}

/* Read position along the Y plate: drive Y+/Y- as the gradient, sample X+. */
static int read_y_raw(void)
{
    pin_hiz(BSP_PIN_TOUCH_XP);   /* sampled by ADC */
    pin_hiz(BSP_PIN_TOUCH_XM);
    pin_out(BSP_PIN_TOUCH_YP, 1);
    pin_out(BSP_PIN_TOUCH_YM, 0);
    esp_rom_delay_us(50);
    return adc_get(BSP_TOUCH_CH_XP);
}

/* Pressure (Z) probe: drive X+ low and Y- high, then sample X- and Y+ with
 * internal pull-downs. A real contact closes the two layers into a divider so
 * Y+ (z2) reads near VDD while X- (z1) stays low -> a large (z2 - z1) spread.
 * An OPEN (unwired or untouched) panel is pulled to ~0 on both, so the spread
 * collapses to zero and no phantom touch is reported. */
static int read_z_raw(void)
{
    pin_out(BSP_PIN_TOUCH_XP, 0);
    pin_out(BSP_PIN_TOUCH_YM, 1);
    pin_pd(BSP_PIN_TOUCH_XM);    /* sampled, pulled low when open */
    pin_pd(BSP_PIN_TOUCH_YP);    /* sampled, pulled low when open */
    esp_rom_delay_us(50);
    int z1 = adc_get(BSP_TOUCH_CH_XM);
    int z2 = adc_get(BSP_TOUCH_CH_YP);
    if (z1 < 0 || z2 < 0) {
        return 0;
    }
    int z = z2 - z1;             /* large under a firm press, ~0 when open */
    return z > 0 ? z : 0;
}

static int map_axis(int raw, int in_min, int in_max, int out_span, int invert)
{
    if (in_max == in_min) {
        return 0;
    }
    int v = (raw - in_min) * (out_span - 1) / (in_max - in_min);
    if (v < 0) v = 0;
    if (v > out_span - 1) v = out_span - 1;
    return invert ? (out_span - 1 - v) : v;
}

/* Map a raw X/Y sample pair to screen pixels honouring the calibration + the
 * landscape orientation swaps in touch.h. */
static void to_screen(int rx, int ry, int16_t *sx, int16_t *sy)
{
    int w = ili9341_width();
    int h = ili9341_height();
#if TOUCH_SWAP_XY
    *sx = map_axis(ry, TOUCH_CAL_Y_MIN, TOUCH_CAL_Y_MAX, w, TOUCH_INVERT_X);
    *sy = map_axis(rx, TOUCH_CAL_X_MIN, TOUCH_CAL_X_MAX, h, TOUCH_INVERT_Y);
#else
    *sx = map_axis(rx, TOUCH_CAL_X_MIN, TOUCH_CAL_X_MAX, w, TOUCH_INVERT_X);
    *sy = map_axis(ry, TOUCH_CAL_Y_MIN, TOUCH_CAL_Y_MAX, h, TOUCH_INVERT_Y);
#endif
}

/* ------------------------------------------------------------ sampling task */
#if TOUCH_SIMULATE
/* A scripted tour of the menu (screen coords matching the GUI layout), one tap
 * emitted per cycle so the whole UI can be shown with no touch panel wired. */
static const touch_tap_t s_script[] = {
    {290, 10},  /* header MENU  -> open menu      */
    {160, 57},  /* SENSORS                        */
    {290, 10},  /* BACK -> menu                   */
    {160, 91},  /* MODE CTRL                      */
    {290, 10},  /* BACK -> menu                   */
    {160, 159}, /* ABOUT                          */
    {290, 10},  /* BACK -> menu                   */
    {290, 10},  /* BACK -> dashboard              */
};

static void touch_task(void *arg)
{
    (void) arg;
    size_t i = 0;
    ESP_LOGW(TAG, "TOUCH_SIMULATE on: injecting scripted taps (no ADC reads)");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(3500));
        touch_tap_t tap = s_script[i];
        i = (i + 1) % (sizeof s_script / sizeof s_script[0]);
        xQueueSend(s_tapq, &tap, 0);
    }
}
#else
static void touch_task(void *arg)
{
    (void) arg;
    int      streak = 0;      /* consecutive pressed samples */
    bool     latched = false; /* a tap for this press already emitted */
    TickType_t next = xTaskGetTickCount();

    for (;;) {
        int z = read_z_raw();
        bool pressed = z > TOUCH_Z_THRESHOLD;

        if (pressed) {
            int rx = read_x_raw();
            int ry = read_y_raw();
            if (rx < 0 || ry < 0) {
                pressed = false;
            } else if (++streak >= TOUCH_DEBOUNCE_N && !latched) {
                touch_tap_t tap;
                to_screen(rx, ry, &tap.x, &tap.y);
                xQueueSend(s_tapq, &tap, 0);
                latched = true;   /* one tap per press; re-arm on release */
            }
        }
        if (!pressed) {
            streak = 0;
            latched = false;
        }
        vTaskDelayUntil(&next, pdMS_TO_TICKS(TOUCH_TASK_PERIOD_MS));
    }
}
#endif

/* ------------------------------------------------------------------- public */

obc_err_t touch_init(void)
{
    s_tapq = xQueueCreate(4, sizeof(touch_tap_t));
    if (s_tapq == NULL) {
        return OBC_ERR_NO_MEM;
    }

#if !TOUCH_SIMULATE
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = BSP_TOUCH_ADC_UNIT };
    if (adc_oneshot_new_unit(&ucfg, &s_adc) != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit init failed");
        return OBC_ERR_FAIL;
    }
    adc_oneshot_chan_cfg_t ccfg = {
        .atten = ADC_ATTEN_DB_12,       /* full-scale ~0..3.3 V */
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_oneshot_config_channel(s_adc, BSP_TOUCH_CH_XP, &ccfg);
    adc_oneshot_config_channel(s_adc, BSP_TOUCH_CH_YP, &ccfg);
    adc_oneshot_config_channel(s_adc, BSP_TOUCH_CH_XM, &ccfg);
    ESP_LOGI(TAG, "touch ADC ready (X+=%d Y+=%d X-=%d Y-=%d)",
             BSP_PIN_TOUCH_XP, BSP_PIN_TOUCH_YP, BSP_PIN_TOUCH_XM, BSP_PIN_TOUCH_YM);
#endif
    return OBC_OK;
}

obc_err_t touch_start(void)
{
    if (xTaskCreate(touch_task, "touch", TOUCH_STACK, NULL, TOUCH_PRIO, NULL) != pdPASS) {
        return OBC_ERR_NO_MEM;
    }
    return OBC_OK;
}

QueueHandle_t touch_tap_queue(void)
{
    return s_tapq;
}
