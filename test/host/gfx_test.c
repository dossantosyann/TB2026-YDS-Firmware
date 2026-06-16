/* Host unit test for the gfx framebuffer renderer. No ESP-IDF, no hardware:
   draws a known pattern, asserts specific pixels (clipping, draw order,
   outline-vs-fill), and dumps the framebuffer to gfx_test.ppm to eyeball. */
#include <stdio.h>
#include <stdint.h>
#include "esp_err.h"
#include "gfx.h"

/* gfx_flush() calls into the display driver; stub it on host. */
esp_err_t display_oled_draw(const uint8_t *buf, size_t len) { (void)buf; (void)len; return ESP_OK; }

static int failures = 0;

static void expect(const char *name, int x, int y, gfx_color_t want)
{
    gfx_color_t got = gfx_buffer()[y * GFX_W + x];
    if (got != want) {
        printf("  FAIL %-18s (%3d,%3d): got 0x%04X want 0x%04X\n", name, x, y, got, want);
        failures++;
    }
}

int main(void)
{
    const gfx_color_t bg    = gfx_rgb(0, 0, 40);
    const gfx_color_t red   = gfx_rgb(200, 40, 40);
    const gfx_color_t green = gfx_rgb(40, 200, 40);

    gfx_clear(bg);
    gfx_fill_rect(20, 20, 60, 40, red);          /* solid block            */
    gfx_draw_rect(100, 30, 50, 50, GFX_WHITE);   /* 1px outline (not fill)  */
    gfx_fill_rect(-10, 100, 40, 30, green);      /* clipped at left edge    */
    gfx_fill_rect(150, 150, 50, 50, gfx_rgb(40, 40, 200)); /* clipped bottom-right */
    for (int i = 0; i < GFX_W; i++) gfx_pixel(i, i, GFX_WHITE); /* diagonal, drawn last */
    gfx_flush();

    expect("background", 5, 80, bg);   /* clear of every rect, diagonal, and the green clip */
    expect("fill_rect",  40, 30, red);
    expect("outline_edge", 120, 30, GFX_WHITE);  /* top edge of the outlined rect */
    expect("outline_hollow", 120, 50, bg);       /* interior stays background     */
    expect("clip_left", 0, 110, green);          /* survived the negative-x clip  */
    expect("diagonal_on_top", 70, 70, GFX_WHITE);/* diagonal overwrote background */

    gfx_draw_text(2, 158, "I", GFX_WHITE, 1);    /* render a glyph in an empty area */
    expect("glyph_lit", 4, 161, GFX_WHITE);      /* 'I' centre column is lit        */
    expect("glyph_gap", 2, 161, bg);             /* its empty left column stays bg   */

    FILE *f = fopen("gfx_test.ppm", "wb");
    fprintf(f, "P6\n%d %d\n255\n", GFX_W, GFX_H);
    for (int i = 0; i < GFX_W * GFX_H; i++) {
        gfx_color_t c = gfx_buffer()[i];
        fputc(((c >> 11) & 0x1F) * 255 / 31, f);
        fputc(((c >> 5)  & 0x3F) * 255 / 63, f);
        fputc(( c        & 0x1F) * 255 / 31, f);
    }
    fclose(f);

    printf(failures ? "%d assertion(s) FAILED\n" : "all assertions passed; wrote gfx_test.ppm\n",
           failures);
    return failures ? 1 : 0;
}
