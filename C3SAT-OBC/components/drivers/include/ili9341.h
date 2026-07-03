/**
 * @file ili9341.h
 * @brief Minimal ILI9341 panel transport over 4-wire SPI.
 *
 * Drives the MI0283QT-9A (ILI9341) panel of the MikroE TFT Proto (MIKROE-495)
 * board using SPI only — the DB0..DB17 parallel bus is intentionally unused.
 * NOTE: the board's serial clock is the silkscreen "WR" pin (no "SCLK" label);
 * the IM0..IM3 straps must select 4-wire 8-bit serial (see bsp_pins.h). This module
 * is the low-level transport: reset, init sequence, address-window setup and
 * pixel pushing. All shapes/text live in the gfx layer on top of it.
 *
 * Colours are 16-bit RGB565, big-endian on the wire (ILI9341 default).
 */
#ifndef ILI9341_H
#define ILI9341_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Display orientation; sets the ILI9341 MADCTL register. */
typedef enum {
    ILI9341_ROT_PORTRAIT = 0,
    ILI9341_ROT_LANDSCAPE,
    ILI9341_ROT_PORTRAIT_FLIP,
    ILI9341_ROT_LANDSCAPE_FLIP,
} ili9341_rot_t;

/** Reset, configure and switch the panel on. Requires hal_spi_init() first. */
obc_err_t ili9341_init(ili9341_rot_t rot);

/** Active drawing width after rotation. */
uint16_t ili9341_width(void);
/** Active drawing height after rotation. */
uint16_t ili9341_height(void);

/**
 * @brief Set the rectangular GRAM window subsequent pixel writes fill.
 * Coordinates are inclusive.
 */
obc_err_t ili9341_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Stream @p count RGB565 pixels into the currently set window.
 *
 * Pixels are sent via DMA; this blocks until the transfer completes.
 */
obc_err_t ili9341_write_pixels(const uint16_t *pixels, size_t count);

/** Fill a rectangle with a solid colour (efficient, chunked DMA). */
obc_err_t ili9341_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/** Turn the backlight on/off. */
void ili9341_backlight(bool on);

#ifdef __cplusplus
}
#endif

#endif /* ILI9341_H */
