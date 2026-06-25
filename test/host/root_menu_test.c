/* Host unit test for root_menu's render(). No ESP-IDF, no hardware:
   renders the home screen and asserts the 3 white tiles are drawn at the
   expected centered positions, each with a black icon inside, then dumps
   root_menu_test.ppm to eyeball. */
#include <stdio.h>
#include <stdint.h>
#include "esp_err.h"
#include "gfx.h"
#include "root_menu.h"

/* gfx.c (gfx_flush) pulls this in; stub the blit. */
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

/* Is any pixel of `want` present in the rect? Confirms an icon drew. */
static void has_pixel(const char *name, int x0, int y0, int x1, int y1, gfx_color_t want)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if (px(x, y) == want) return;
    printf("  FAIL %-16s no 0x%04X pixel in [%d,%d]-[%d,%d]\n", name, want, x0, y0, x1, y1);
    failures++;
}

int main(void)
{
    screen_t *s = root_menu();
    s->render(s);

    /* Geometry mirrors root_menu.c: TILE 36, GAP 12, 3 tiles.
       row_w=132, x0=22, y=70; tile i at x = 22 + i*48; corner brackets 2px/8px,
       icon inset 2px. */
    static const int tile_x[3] = { 22, 70, 118 };
    const int y = 70;
    const int TILE = 36;

    expect("bg_black", 0, 0, GFX_BLACK);   /* outside the row stays black */

    for (int i = 0; i < 3; i++) {
        int x = tile_x[i];
        /* Corner brackets: the four corners are white... */
        expect("corner_tl", x,            y,            GFX_WHITE);
        expect("corner_br", x + TILE - 1, y + TILE - 1, GFX_WHITE);
        /* ...but the middle of the top/bottom edge is black: no long horizontal
           line. This is the whole point of the redesign (kills the row halo). */
        expect("top_mid_black", x + TILE / 2, y,            GFX_BLACK);
        expect("bot_mid_black", x + TILE / 2, y + TILE - 1, GFX_BLACK);
        /* Interior is mostly black, with the icon in white. */
        has_pixel("interior_black", x + 2, y + 2, x + 34, y + 34, GFX_BLACK);
        has_pixel("icon_white",     x + 2, y + 2, x + 34, y + 34, GFX_WHITE);
    }

    FILE *f = fopen("root_menu_test.ppm", "wb");
    fprintf(f, "P6\n%d %d\n255\n", GFX_W, GFX_H);
    for (int i = 0; i < GFX_W * GFX_H; i++) {
        gfx_color_t c = gfx_buffer()[i];
        fputc(((c >> 11) & 0x1F) * 255 / 31, f);
        fputc(((c >> 5)  & 0x3F) * 255 / 63, f);
        fputc(( c        & 0x1F) * 255 / 31, f);
    }
    fclose(f);

    printf(failures ? "%d assertion(s) FAILED\n"
                    : "root_menu: all assertions passed; wrote root_menu_test.ppm\n",
           failures);
    return failures ? 1 : 0;
}
