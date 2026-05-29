#include "gfx.h"
#include "font5x7.h"
#include "ili9341.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

/* Largest glyph cell we blit at once: 6 cols * 8 rows * scale^2, scale<=3. */
#define GLYPH_MAX_PX (6 * 8 * 3 * 3)
static uint16_t s_glyph[GLYPH_MAX_PX];

obc_err_t gfx_init(void)
{
    return ili9341_init(ILI9341_ROT_LANDSCAPE);
}

void gfx_clear(uint16_t color)
{
    ili9341_fill_rect(0, 0, ili9341_width(), ili9341_height(), color);
}

void gfx_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0) return;
    ili9341_fill_rect((uint16_t) x, (uint16_t) y, (uint16_t) w, (uint16_t) h, color);
}

void gfx_hline(int x, int y, int w, uint16_t color)
{
    gfx_fill_rect(x, y, w, 1, color);
}

void gfx_rect(int x, int y, int w, int h, uint16_t color)
{
    gfx_fill_rect(x, y, w, 1, color);
    gfx_fill_rect(x, y + h - 1, w, 1, color);
    gfx_fill_rect(x, y, 1, h, color);
    gfx_fill_rect(x + w - 1, y, 1, h, color);
}

/* Render one glyph into s_glyph (cell = 6*scale wide, 8*scale tall) and blit. */
static void draw_glyph(int x, int y, char ch, uint16_t fg, uint16_t bg, int scale)
{
    if (scale < 1) scale = 1;
    if (scale > 3) scale = 3;

    unsigned char c = (unsigned char) toupper((unsigned char) ch);
    if (c < FONT5X7_FIRST || c > FONT5X7_LAST) {
        c = ' ';
    }
    const uint8_t *glyph = font5x7[c - FONT5X7_FIRST];

    int cw = 6 * scale;  /* 5 cols + 1 spacing */
    int chh = 8 * scale; /* 7 rows + 1 spacing */

    for (int col = 0; col < 6; ++col) {
        uint8_t bits = (col < 5) ? glyph[col] : 0x00;
        for (int row = 0; row < 8; ++row) {
            uint16_t px = (bits & (1u << row)) ? fg : bg;
            /* expand this font pixel to scale x scale block */
            for (int sy = 0; sy < scale; ++sy) {
                for (int sx = 0; sx < scale; ++sx) {
                    int bx = col * scale + sx;
                    int by = row * scale + sy;
                    s_glyph[by * cw + bx] = px;
                }
            }
        }
    }
    ili9341_set_window((uint16_t) x, (uint16_t) y,
                       (uint16_t) (x + cw - 1), (uint16_t) (y + chh - 1));
    ili9341_write_pixels(s_glyph, (size_t) (cw * chh));
}

void gfx_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale)
{
    int cw = gfx_char_w(scale);
    for (const char *p = s; *p; ++p) {
        draw_glyph(x, y, *p, fg, bg, scale);
        x += cw;
    }
}

void gfx_printf(int x, int y, uint16_t fg, uint16_t bg, int scale, const char *fmt, ...)
{
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    gfx_text(x, y, buf, fg, bg, scale);
}

void gfx_bar(int x, int y, int w, int h, float value, uint16_t fg, uint16_t bg)
{
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    gfx_rect(x, y, w, h, GFX_GREY);
    int inner = w - 2;
    int filled = (int) (inner * value);
    gfx_fill_rect(x + 1, y + 1, filled, h - 2, fg);
    gfx_fill_rect(x + 1 + filled, y + 1, inner - filled, h - 2, bg);
}
