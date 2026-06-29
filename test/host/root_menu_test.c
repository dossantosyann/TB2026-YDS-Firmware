/* Host unit test for root_menu's render(). No ESP-IDF, no hardware:
   renders the home screen (a vertical list of 4 icon+label rows) and asserts the
   geometry -- icon and label present on every row, white corner brackets only on
   the selected row (with black mid-edges, the whole point of the passive-OLED
   redesign), and no brackets on the others -- then dumps root_menu_test.ppm. */
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

/* Is any pixel of `want` present in the rect? Confirms an icon/label drew. */
static void has_pixel(const char *name, int x0, int y0, int x1, int y1, gfx_color_t want)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if (px(x, y) == want) return;
    printf("  FAIL %-16s no 0x%04X pixel in [%d,%d]-[%d,%d]\n", name, want, x0, y0, x1, y1);
    failures++;
}

/* Is any pixel neither black nor white in the rect? Confirms a colored (accent)
   icon drew, without pinning the exact palette (which is user-tweakable). */
static void has_color(const char *name, int x0, int y0, int x1, int y1)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            gfx_color_t c = px(x, y);
            if (c != GFX_BLACK && c != GFX_WHITE) return;
        }
    printf("  FAIL %-16s no colored pixel in [%d,%d]-[%d,%d]\n", name, x0, y0, x1, y1);
    failures++;
}

int main(void)
{
    screen_t *s = root_menu();
    s->render(s);   /* default selection is row 0 (Music) */

    /* Geometry mirrors root_menu.c: 4 rows of 44px; icon 32x32 at x=8, inset 6px;
       label at x=48 scale 2; corner brackets 2px thick / 8px arms, full width. */
    const int ROW_H = 44;

    for (int i = 0; i < 4; i++) {
        int y = i * ROW_H;
        /* Every row shows its label in white, selected or not. */
        has_pixel("label_white", 48, y + 12, 144, y + 32, GFX_WHITE);

        if (i == 0) {
            /* Selected row: icon is drawn in its accent color (not white). */
            has_color("icon_accent", 8, y + 6, 40, y + 38);
            /* ...and the four corners are white... */
            expect("corner_tl", 0,          y,             GFX_WHITE);
            expect("corner_tr", GFX_W - 1,  y,             GFX_WHITE);
            expect("corner_bl", 0,          y + ROW_H - 1, GFX_WHITE);
            expect("corner_br", GFX_W - 1,  y + ROW_H - 1, GFX_WHITE);
            /* ...but the middle of the top/bottom edge is black: no long horizontal
               line, which is the whole point of the redesign (kills the row halo). */
            expect("top_mid_black", GFX_W / 2, y,             GFX_BLACK);
            expect("bot_mid_black", GFX_W / 2, y + ROW_H - 1, GFX_BLACK);
        } else {
            /* Unselected rows: icon stays white, no decoration (corners black). */
            has_pixel("icon_white", 8, y + 6, 40, y + 38, GFX_WHITE);
            expect("nosel_tl", 0,         y,             GFX_BLACK);
            expect("nosel_tr", GFX_W - 1, y,             GFX_BLACK);
            expect("nosel_bl", 0,         y + ROW_H - 1, GFX_BLACK);
            expect("nosel_br", GFX_W - 1, y + ROW_H - 1, GFX_BLACK);
        }
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
