/**
 * @file hal_i2c.h
 * @brief Thread-safe I2C master bus shared by every sensor driver.
 *
 * The EPS monitor, IMU, thermal sensor and RTC all hang off one I2C bus. With
 * several FreeRTOS tasks polling them at different rates, concurrent access
 * must be serialised — otherwise two register transactions interleave and
 * corrupt each other. This HAL owns the bus and guards every transfer with a
 * recursive mutex, exposing simple register-level helpers to the drivers.
 */
#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>
#include <stddef.h>
#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise the shared I2C master bus (idempotent). */
obc_err_t hal_i2c_init(void);

/**
 * @brief Check whether a device ACKs at the given 7-bit address.
 * @return OBC_OK if present, OBC_ERR_NO_DEVICE otherwise.
 */
obc_err_t hal_i2c_probe(uint8_t addr7);

/** Read @p len bytes starting at register @p reg. Mutex-guarded. */
obc_err_t hal_i2c_read_reg(uint8_t addr7, uint8_t reg, uint8_t *buf, size_t len);

/** Write @p len bytes starting at register @p reg. Mutex-guarded. */
obc_err_t hal_i2c_write_reg(uint8_t addr7, uint8_t reg, const uint8_t *buf, size_t len);

/** Convenience: write a single register byte. */
obc_err_t hal_i2c_write_u8(uint8_t addr7, uint8_t reg, uint8_t val);

/** Convenience: read a single register byte. */
obc_err_t hal_i2c_read_u8(uint8_t addr7, uint8_t reg, uint8_t *val);

/** Read a big-endian 16-bit register (MSB at @p reg). */
obc_err_t hal_i2c_read_u16_be(uint8_t addr7, uint8_t reg, uint16_t *val);

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C_H */
