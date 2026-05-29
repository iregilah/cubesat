#include "hal_i2c.h"
#include "bsp_pins.h"

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "hal_i2c";

#define I2C_TIMEOUT_MS  50

static i2c_master_bus_handle_t s_bus;
static SemaphoreHandle_t       s_lock;     /* serialises all bus access */
static bool                    s_ready;

/* Map an esp_err_t to our error space. */
static obc_err_t map_err(esp_err_t e)
{
    switch (e) {
        case ESP_OK:             return OBC_OK;
        case ESP_ERR_TIMEOUT:    return OBC_ERR_TIMEOUT;
        case ESP_ERR_NOT_FOUND:  return OBC_ERR_NO_DEVICE;
        case ESP_ERR_INVALID_ARG:return OBC_ERR_INVALID_ARG;
        default:                 return OBC_ERR_FAIL;
    }
}

obc_err_t hal_i2c_init(void)
{
    if (s_ready) {
        return OBC_OK;
    }
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return OBC_ERR_NO_MEM;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = BSP_I2C_PORT,
        .sda_io_num = BSP_PIN_I2C_SDA,
        .scl_io_num = BSP_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bus init failed: %s", esp_err_to_name(err));
        return map_err(err);
    }
    s_ready = true;
    ESP_LOGI(TAG, "I2C%d up on SDA=%d SCL=%d @ %d Hz",
             BSP_I2C_PORT, BSP_PIN_I2C_SDA, BSP_PIN_I2C_SCL, BSP_I2C_HZ);
    return OBC_OK;
}

/* Add a transient device handle for one transaction. Caller holds s_lock. */
static esp_err_t add_dev(uint8_t addr7, i2c_master_dev_handle_t *dev)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr7,
        .scl_speed_hz = BSP_I2C_HZ,
    };
    return i2c_master_bus_add_device(s_bus, &dev_cfg, dev);
}

obc_err_t hal_i2c_probe(uint8_t addr7)
{
    if (!s_ready) {
        return OBC_ERR_FAIL;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = i2c_master_probe(s_bus, addr7, I2C_TIMEOUT_MS);
    xSemaphoreGive(s_lock);
    return (err == ESP_OK) ? OBC_OK : OBC_ERR_NO_DEVICE;
}

obc_err_t hal_i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *buf, size_t len)
{
    if (!s_ready || buf == NULL) {
        return OBC_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    i2c_master_dev_handle_t dev;
    esp_err_t err = add_dev(addr7, &dev);
    if (err == ESP_OK) {
        /* Write the register pointer, then read the payload in one bus turn. */
        err = i2c_master_transmit_receive(dev, &reg, 1, buf, len, I2C_TIMEOUT_MS);
        i2c_master_bus_rm_device(dev);
    }
    xSemaphoreGive(s_lock);
    return map_err(err);
}

obc_err_t hal_i2c_write_reg(uint8_t addr7, uint8_t reg, const uint8_t *buf, size_t len)
{
    if (!s_ready || (len > 0 && buf == NULL) || len > 32) {
        return OBC_ERR_INVALID_ARG;
    }
    uint8_t tx[33];
    tx[0] = reg;
    for (size_t i = 0; i < len; ++i) {
        tx[i + 1] = buf[i];
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    i2c_master_dev_handle_t dev;
    esp_err_t err = add_dev(addr7, &dev);
    if (err == ESP_OK) {
        err = i2c_master_transmit(dev, tx, len + 1, I2C_TIMEOUT_MS);
        i2c_master_bus_rm_device(dev);
    }
    xSemaphoreGive(s_lock);
    return map_err(err);
}

obc_err_t hal_i2c_write_u8(uint8_t addr7, uint8_t reg, uint8_t val)
{
    return hal_i2c_write_reg(addr7, reg, &val, 1);
}

obc_err_t hal_i2c_read_u8(uint8_t addr7, uint8_t reg, uint8_t *val)
{
    return hal_i2c_read_reg(addr7, reg, val, 1);
}

obc_err_t hal_i2c_read_u16_be(uint8_t addr7, uint8_t reg, uint16_t *val)
{
    uint8_t b[2];
    obc_err_t rc = hal_i2c_read_reg(addr7, reg, b, 2);
    if (rc == OBC_OK) {
        *val = ((uint16_t) b[0] << 8) | b[1];
    }
    return rc;
}
