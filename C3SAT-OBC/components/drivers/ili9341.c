#include "ili9341.h"
#include "bsp_pins.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ili9341";

/* ILI9341 command set (subset we use). */
#define CMD_SWRESET 0x01
#define CMD_SLPOUT  0x11
#define CMD_DISPON  0x29
#define CMD_CASET   0x2A
#define CMD_PASET   0x2B
#define CMD_RAMWR   0x2C
#define CMD_MADCTL  0x36
#define CMD_COLMOD  0x3A

/* MADCTL bits. */
#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_BGR 0x08

#define PIXEL_CHUNK 256  /* fill_rect DMA staging size, in pixels */

static spi_device_handle_t s_spi;
static uint16_t s_w = BSP_LCD_WIDTH;
static uint16_t s_h = BSP_LCD_HEIGHT;
static uint16_t s_fillbuf[PIXEL_CHUNK];

/* Pre-transaction callback: drive the D/C line from the per-transfer user tag.
 * Runs in SPI ISR context, so it must stay tiny. user != NULL => data phase. */
static void IRAM_ATTR pre_cb(spi_transaction_t *t)
{
    gpio_set_level(BSP_PIN_LCD_DC, t->user ? 1 : 0);
}

static esp_err_t spi_tx(const uint8_t *data, size_t len, bool is_data)
{
    if (len == 0) {
        return ESP_OK;
    }
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
        .user = (void *) (is_data ? 1 : 0),
    };
    return spi_device_polling_transmit(s_spi, &t);
}

static esp_err_t write_cmd(uint8_t cmd)
{
    return spi_tx(&cmd, 1, false);
}

static esp_err_t write_data(const uint8_t *data, size_t len)
{
    return spi_tx(data, len, true);
}

static void hw_reset(void)
{
    gpio_set_level(BSP_PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(BSP_PIN_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(BSP_PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(150));
}

static uint8_t madctl_for(ili9341_rot_t rot)
{
    switch (rot) {
        case ILI9341_ROT_LANDSCAPE:
            s_w = 320; s_h = 240; return MADCTL_MV | MADCTL_BGR;
        case ILI9341_ROT_LANDSCAPE_FLIP:
            s_w = 320; s_h = 240; return MADCTL_MV | MADCTL_MX | MADCTL_MY | MADCTL_BGR;
        case ILI9341_ROT_PORTRAIT_FLIP:
            s_w = 240; s_h = 320; return MADCTL_MX | MADCTL_MY | MADCTL_BGR;
        case ILI9341_ROT_PORTRAIT:
        default:
            s_w = 240; s_h = 320; return MADCTL_MX | MADCTL_BGR;
    }
}

obc_err_t ili9341_init(ili9341_rot_t rot)
{
    /* Control GPIOs (DC/RST/BL) are plain outputs; CS is handled by the SPI
     * peripheral as the chip-select for this device. */
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << BSP_PIN_LCD_DC) |
                        (1ULL << BSP_PIN_LCD_RST) |
                        (1ULL << BSP_PIN_LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);

    spi_device_interface_config_t dev = {
        .clock_speed_hz = BSP_LCD_SPI_HZ,
        .mode = 0,
        .spics_io_num = BSP_PIN_LCD_CS,
        .queue_size = 7,
        .pre_cb = pre_cb,
        .flags = SPI_DEVICE_NO_DUMMY,
    };
    esp_err_t err = spi_bus_add_device(BSP_SPI_HOST, &dev, &s_spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add_device failed: %s", esp_err_to_name(err));
        return OBC_ERR_FAIL;
    }

    hw_reset();

    write_cmd(CMD_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(120));
    write_cmd(CMD_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    write_cmd(CMD_COLMOD);
    write_data((const uint8_t[]){0x55}, 1); /* 16 bits/pixel */

    write_cmd(CMD_MADCTL);
    uint8_t madctl = madctl_for(rot);
    write_data(&madctl, 1);

    write_cmd(CMD_DISPON);
    vTaskDelay(pdMS_TO_TICKS(50));

    ili9341_backlight(true);
    ESP_LOGI(TAG, "panel up %ux%u", s_w, s_h);
    return OBC_OK;
}

uint16_t ili9341_width(void)  { return s_w; }
uint16_t ili9341_height(void) { return s_h; }

obc_err_t ili9341_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t cols[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    uint8_t rows[4] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };
    write_cmd(CMD_CASET);
    write_data(cols, 4);
    write_cmd(CMD_PASET);
    write_data(rows, 4);
    write_cmd(CMD_RAMWR);
    return OBC_OK;
}

obc_err_t ili9341_write_pixels(const uint16_t *pixels, size_t count)
{
    /* ILI9341 expects big-endian RGB565; swap into the staging buffer in
     * chunks so we never need a frame-sized DMA allocation. */
    while (count) {
        size_t n = count < PIXEL_CHUNK ? count : PIXEL_CHUNK;
        for (size_t i = 0; i < n; ++i) {
            s_fillbuf[i] = (pixels[i] >> 8) | (pixels[i] << 8);
        }
        if (write_data((const uint8_t *) s_fillbuf, n * 2) != ESP_OK) {
            return OBC_ERR_FAIL;
        }
        pixels += n;
        count  -= n;
    }
    return OBC_OK;
}

obc_err_t ili9341_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= s_w || y >= s_h) {
        return OBC_ERR_INVALID_ARG;
    }
    if (x + w > s_w) w = s_w - x;
    if (y + h > s_h) h = s_h - y;

    uint16_t be = (color >> 8) | (color << 8);
    for (size_t i = 0; i < PIXEL_CHUNK; ++i) {
        s_fillbuf[i] = be;
    }

    ili9341_set_window(x, y, x + w - 1, y + h - 1);
    size_t total = (size_t) w * h;
    while (total) {
        size_t n = total < PIXEL_CHUNK ? total : PIXEL_CHUNK;
        if (write_data((const uint8_t *) s_fillbuf, n * 2) != ESP_OK) {
            return OBC_ERR_FAIL;
        }
        total -= n;
    }
    return OBC_OK;
}

void ili9341_backlight(bool on)
{
    gpio_set_level(BSP_PIN_LCD_BL, on ? 1 : 0);
}
