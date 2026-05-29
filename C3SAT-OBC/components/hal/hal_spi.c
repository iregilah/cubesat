#include "hal_spi.h"
#include "bsp_pins.h"

#include "driver/spi_master.h"
#include "esp_log.h"

static const char *TAG = "hal_spi";
static bool s_ready;

obc_err_t hal_spi_init(void)
{
    if (s_ready) {
        return OBC_OK;
    }
    spi_bus_config_t cfg = {
        .sclk_io_num = BSP_PIN_SPI_SCLK,
        .mosi_io_num = BSP_PIN_SPI_MOSI,
        .miso_io_num = BSP_PIN_SPI_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        /* One full 16-bit-colour scan line is the largest single transfer the
         * display driver pushes; size the DMA buffer to fit it. */
        .max_transfer_sz = BSP_LCD_WIDTH * 2 + 16,
    };
    esp_err_t err = spi_bus_initialize(BSP_SPI_HOST, &cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bus init failed: %s", esp_err_to_name(err));
        return OBC_ERR_FAIL;
    }
    s_ready = true;
    ESP_LOGI(TAG, "SPI bus up: SCLK=%d MOSI=%d MISO=%d",
             BSP_PIN_SPI_SCLK, BSP_PIN_SPI_MOSI, BSP_PIN_SPI_MISO);
    return OBC_OK;
}
