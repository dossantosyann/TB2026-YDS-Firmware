#pragma once

#include <stdint.h>

/* Logical canvas = the SSD1333 panel: 176x176, RGB565. */
#define GFX_W 176
#define GFX_H 176

typedef uint16_t gfx_color_t;   /* RGB565, native byte order */

/* Pack 8-bit RGB into RGB565. */
static inline gfx_color_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (gfx_color_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

#define GFX_BLACK ((gfx_color_t)0x0000)
#define GFX_WHITE ((gfx_color_t)0xFFFF)

/* Built-in 5x7 font, drawn in a 6x8 cell (1px gap). Multiply by scale for size. */
#define GFX_CHAR_W 6
#define GFX_CHAR_H 8

void gfx_clear(gfx_color_t color);
void gfx_pixel(int x, int y, gfx_color_t color);
void gfx_fill_rect(int x, int y, int w, int h, gfx_color_t color);
void gfx_draw_rect(int x, int y, int w, int h, gfx_color_t color);   /* 1px outline */
void gfx_draw_text(int x, int y, const char *s, gfx_color_t color, int scale);
void gfx_flush(void);

/* The framebuffer (GFX_W*GFX_H px). Used by the flush path and by host tests. */
const gfx_color_t *gfx_buffer(void);
