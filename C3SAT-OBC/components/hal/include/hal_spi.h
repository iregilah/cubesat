/**
 * @file hal_spi.h
 * @brief Shared SPI master bus used by the display driver.
 *
 * Only the ILI9341 panel lives on this bus today, but keeping bus
 * initialisation here (separate from the device driver) mirrors the I2C HAL
 * and leaves room to add further SPI peripherals (e.g. an SD card for mass
 * memory) without touching the display code.
 */
#ifndef HAL_SPI_H
#define HAL_SPI_H

#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise the shared SPI master bus (idempotent). */
obc_err_t hal_spi_init(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SPI_H */
