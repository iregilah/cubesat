/**
 * @file gfx.h
 * @brief Tiny RGB565 drawing layer on top of the ILI9341 transport.
 *
 * No full framebuffer (the C6 has limited RAM): primitives compose small
 * glyph/​span buffers and blit them, which is plenty for a text dashboard.
 */
#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include "obc_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Common RGB565 colours. */
#define GFX_BLACK   0x0000
#define GFX_WHITE   0xFFFF
#define GFX_RED     0xF800
#define GFX_GREEN   0x07E0
#define GFX_BLUE    0x001F
#define GFX_YELLOW  0xFFE0
#define GFX_CYAN    0x07FF
#define GFX_MAGENTA 0xF81F
#define GFX_ORANGE  0xFD20
#define GFX_GREY    0x8410
#define GFX_DKGREY  0x39E7
#define GFX_NAVY    0x000F

/** Pack 8-bit RGB into RGB565. */
static inline uint16_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

obc_err_t gfx_init(void);
void gfx_clear(uint16_t color);
void gfx_fill_rect(int x, int y, int w, int h, uint16_t color);
/** Single-pixel-thick rectangle outline. */
void gfx_rect(int x, int y, int w, int h, uint16_t color);
void gfx_hline(int x, int y, int w, uint16_t color);

/**
 * @brief Draw a text string. Lowercase is folded to uppercase (font is 0x20..0x5F).
 * @param scale integer pixel scale (1 = 6x8 cell, 2 = 12x16, ...).
 */
void gfx_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale);
/** printf-style convenience wrapper around gfx_text. */
void gfx_printf(int x, int y, uint16_t fg, uint16_t bg, int scale, const char *fmt, ...);

/** Horizontal progress/level bar with border (value 0..1). */
void gfx_bar(int x, int y, int w, int h, float value, uint16_t fg, uint16_t bg);

/** Advance in pixels of one character cell at the given scale. */
static inline int gfx_char_w(int scale) { return 6 * scale; }
static inline int gfx_char_h(int scale) { return 8 * scale; }

#ifdef __cplusplus
}
#endif

#endif /* GFX_H */
