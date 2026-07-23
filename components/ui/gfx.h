/**
 * @file gfx.h
 * @brief Framebuffer drawing primitives for the 176x176 RGB565 OLED.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#pragma once

#include <stdint.h>

/**
 * @defgroup ui_gfx Graphics
 * @ingroup ui
 * @brief Software rendering into an in-RAM RGB565 framebuffer, blitted by gfx_flush().
 * @{
 */

#define GFX_W 176  /**< Canvas width in pixels (the SSD1333 panel). */
#define GFX_H 176  /**< Canvas height in pixels. */

typedef uint16_t gfx_color_t;   /**< A pixel: RGB565, native byte order. */

/**
 * @brief Pack 8-bit R/G/B channels into an RGB565 color.
 * @param r,g,b  8-bit color channels.
 * @return The packed RGB565 value.
 */
static inline gfx_color_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (gfx_color_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

#define GFX_BLACK ((gfx_color_t)0x0000)  /**< Black. */
#define GFX_WHITE ((gfx_color_t)0xFFFF)  /**< White. */

/* Built-in 5x7 font, drawn in a 6x8 cell (1px gap). Multiply by scale for size. */
#define GFX_CHAR_W 6  /**< Character cell width: 5px glyph + 1px gap. */
#define GFX_CHAR_H 8  /**< Character cell height. */

/** @brief Fill the whole framebuffer with one color. */
void gfx_clear(gfx_color_t color);

/** @brief Set one pixel; off-canvas coordinates are ignored. */
void gfx_pixel(int x, int y, gfx_color_t color);

/** @brief Fill a rectangle, clipped to the canvas. */
void gfx_fill_rect(int x, int y, int w, int h, gfx_color_t color);

/** @brief Draw a 1px rectangle outline. */
void gfx_draw_rect(int x, int y, int w, int h, gfx_color_t color);

/**
 * @brief Draw an ASCII string (0x20..0x7E) left to right.
 * @param x,y    Top-left of the first glyph.
 * @param s      NUL-terminated string; non-printable bytes advance but draw nothing.
 * @param color  Glyph color.
 * @param scale  Integer pixel scale (clamped to >= 1).
 */
void gfx_draw_text(int x, int y, const char *s, gfx_color_t color, int scale);

/**
 * @brief Draw a 1-bpp bitmap; set bits are drawn in @p color, clear bits are skipped.
 *
 * The bitmap is row-major, MSB-first within each byte (bit 7 = leftmost pixel),
 * with each row padded to a whole number of bytes (stride = (w + 7) / 8). This is
 * the format emitted by scripts/icon2c.py. Clear bits leave the framebuffer
 * untouched, so the background shows through (transparency).
 *
 * @param x,y      Top-left position in the framebuffer.
 * @param w,h      Bitmap dimensions in pixels.
 * @param bitmap   1-bpp pixel data, (w + 7) / 8 * h bytes.
 * @param color    Color for set bits.
 */
void gfx_blit_1bpp(int x, int y, int w, int h, const uint8_t *bitmap, gfx_color_t color);

/** @brief Blit the framebuffer to the OLED over SPI. */
void gfx_flush(void);

/**
 * @brief Read-only access to the framebuffer (GFX_W*GFX_H pixels).
 * @return Pointer to the framebuffer; used by the flush path and by host tests.
 */
const gfx_color_t *gfx_buffer(void);

/** @} */
