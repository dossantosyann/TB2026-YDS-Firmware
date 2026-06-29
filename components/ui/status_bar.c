// Permanent battery readout overlaid on every screen (see navigator_tick).

#include "status_bar.h"
#include "gfx.h"
#include "power.h"
#include <stdio.h>
#include <string.h>

/* Owns the top STATUS_BAR_H rows of every frame. Drawn after the active screen
   renders, so it is the last thing in the frame and always wins; it clears its strip
   to black first, so the bar stays clean even if a screen drew into the top. No
   separator line under the bar: a full-width horizontal run lights OFF pixels across
   the whole line on this passive OLED (IR drop on the shared common -- same reason
   root_menu uses corner brackets, not a bar). Text only keeps the lit-pixel count
   (and power) minimal. The value refreshes on the next repaint, i.e. the next input
   event; the UI is event-driven and deliberately does not wake on its own to update
   the percentage. */
#define MARGIN 2   /* px gap from the right edge */

void status_bar_draw(void)
{
    gfx_fill_rect(0, 0, GFX_W, STATUS_BAR_H, GFX_BLACK);   /* reserve a clean strip */

    power_state_t st;
    power_get_state(&st);

    char buf[8];
    if (st.valid) {
        int pct = (int)(st.soc_pct + 0.5f);
        if (pct < 0)   pct = 0;
        if (pct > 100) pct = 100;
        snprintf(buf, sizeof buf, "%d%%", pct);
    } else {
        snprintf(buf, sizeof buf, "--%%");
    }

    int w = (int)strlen(buf) * GFX_CHAR_W;            /* scale 1: one cell per glyph */
    int x = GFX_W - MARGIN - w;
    int y = (STATUS_BAR_H - GFX_CHAR_H) / 2;          /* vertically center in the bar */

    /* Flag a critically low cell in red; otherwise plain white. */
    gfx_color_t col = (st.valid && st.level == POWER_LEVEL_CRITICAL)
                          ? gfx_rgb(255, 0, 0) : GFX_WHITE;
    gfx_draw_text(x, y, buf, col, 1);
}
