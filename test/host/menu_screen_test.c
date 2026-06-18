/* Host unit test for menu_screen's render(). No ESP-IDF, no hardware:
   renders a 3-item menu with the middle row selected, asserts the selected row
   is highlighted (and others aren't) and that labels actually draw, then dumps
   menu_screen_test.ppm to eyeball. */
#include <stdio.h>
#include <stdint.h>
#include "esp_err.h"
#include "gfx.h"
#include "menu_screen.h"

/* gfx.c (gfx_flush) and menu_screen.c (navigation) pull these in; stub the blit. */
esp_err_t display_oled_draw(const uint8_t *buf, size_t len) { (void)buf; (void)len; return ESP_OK; }

static int failures = 0;

static gfx_color_t px(int x, int y) { return gfx_buffer()[y * GFX_W + x]; }

static void expect(const char *name, int x, int y, gfx_color_t want)
{
    gfx_color_t got = px(x, y);
    if (got != want) {
        printf("  FAIL %-16s (%3d,%3d): got 0x%04X want 0x%04X\n", name, x, y, got, want);
        failures++;
    }
}

/* Is any pixel of `want` present in the rect? Used to confirm a label drew without
   pinning to a specific glyph bit. */
static int has_pixel(const char *name, int x0, int y0, int x1, int y1, gfx_color_t want)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if (px(x, y) == want) return 1;
    printf("  FAIL %-16s no 0x%04X pixel in [%d,%d]-[%d,%d]\n", name, want, x0, y0, x1, y1);
    failures++;
    return 0;
}

int main(void)
{
    static const menu_item_t items[] = {
        { .label = "Music" },
        { .label = "Settings" },
        { .label = "About" },
    };
    menu_screen_t ms;
    menu_screen_init(&ms, items, 3);
    ms.selected = 1;                 /* highlight the middle row */

    ms.base.render(&ms.base);        /* draw into the framebuffer */

    /* Geometry mirrors menu_screen.c: row i spans y=[i*22, i*22+22), full-width bar.
       x=170 is past every (short) label, so it reflects only the row background. */
    expect("selected_bar",  170, 33, GFX_WHITE);  /* row 1 highlighted          */
    expect("unselected_bg", 170, 11, GFX_BLACK);  /* row 0 not highlighted       */

    /* Labels render (text starts at x=8): white text on row 0's black bg,
       black text on row 1's white bar. */
    has_pixel("label_row0",     8, 3,  90, 19, GFX_WHITE);
    has_pixel("label_selected", 8, 25, 90, 41, GFX_BLACK);

    FILE *f = fopen("menu_screen_test.ppm", "wb");
    fprintf(f, "P6\n%d %d\n255\n", GFX_W, GFX_H);
    for (int i = 0; i < GFX_W * GFX_H; i++) {
        gfx_color_t c = gfx_buffer()[i];
        fputc(((c >> 11) & 0x1F) * 255 / 31, f);
        fputc(((c >> 5)  & 0x3F) * 255 / 63, f);
        fputc(( c        & 0x1F) * 255 / 31, f);
    }
    fclose(f);

    printf(failures ? "%d assertion(s) FAILED\n"
                    : "menu_screen: all assertions passed; wrote menu_screen_test.ppm\n",
           failures);
    return failures ? 1 : 0;
}
