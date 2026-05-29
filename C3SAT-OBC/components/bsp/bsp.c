#include "bsp.h"
#include "bsp_pins.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "led_strip.h"

static const char *TAG = "bsp";

static uint32_t          s_boot_count;
static led_strip_handle_t s_led;

#define NVS_NS         "obc"
#define NVS_KEY_BOOTS  "boots"

static obc_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* The config "EEPROM" is corrupt or from an older layout: wipe + retry. */
        ESP_LOGW(TAG, "NVS needs erase (%s), reformatting", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return (err == ESP_OK) ? OBC_OK : OBC_ERR_FAIL;
}

static void bump_boot_counter(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        s_boot_count = 0;
        return;
    }
    (void) nvs_get_u32(h, NVS_KEY_BOOTS, &s_boot_count); /* leaves 0 if absent */
    s_boot_count++;
    nvs_set_u32(h, NVS_KEY_BOOTS, s_boot_count);
    nvs_commit(h);
    nvs_close(h);
}

static void init_led(void)
{
    led_strip_config_t strip_cfg = {
        .strip_gpio_num = BSP_PIN_RGB_LED,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .resolution_hz = 10 * 1000 * 1000,
    };
    if (led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led) != ESP_OK) {
        s_led = NULL;
        ESP_LOGW(TAG, "status LED unavailable");
        return;
    }
    led_strip_clear(s_led);
}

obc_err_t bsp_init(void)
{
    obc_err_t rc = init_nvs();
    if (rc != OBC_OK) {
        return rc;
    }
    bump_boot_counter();
    init_led();
    ESP_LOGI(TAG, "boot #%lu", (unsigned long) s_boot_count);
    return OBC_OK;
}

uint32_t bsp_boot_count(void)
{
    return s_boot_count;
}

void bsp_set_mode_led(obc_mode_t mode)
{
    if (s_led == NULL) {
        return;
    }
    /* GRB-ish colour cues per mode. */
    uint8_t r = 0, g = 0, b = 0;
    switch (mode) {
        case OBC_MODE_BOOT:    r = 8;  g = 8;  b = 8;  break; /* white */
        case OBC_MODE_SAFE:    r = 0;  g = 0;  b = 24; break; /* blue */
        case OBC_MODE_NOMINAL: r = 0;  g = 24; b = 0;  break; /* green */
        case OBC_MODE_PAYLOAD: r = 0;  g = 16; b = 16; break; /* cyan */
        case OBC_MODE_FAULT:   r = 32; g = 0;  b = 0;  break; /* red */
        default: break;
    }
    led_strip_set_pixel(s_led, 0, r, g, b);
    led_strip_refresh(s_led);
}
