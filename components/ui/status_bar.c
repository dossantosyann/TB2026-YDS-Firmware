// Permanent battery readout overlaid on every screen (see navigator_tick).

#include "status_bar.h"
#include "gfx.h"
#include "icons.h"
#include "power.h"
#include "volume.h"
#include "bluetooth.h"
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

    /* Far left: Bluetooth radio activity. Colour + waves are the free cue on this RGB565
       panel: a dim antenna when the radio is off, a white antenna when it is on but not
       linked, and a white antenna with radiating waves once an A2DP link is up. No live
       RSSI of the active link is exposed, so "signal" is shown coarsely (present/absent),
       not as a measured strength -- and the bar is event-driven, so it refreshes on the
       next input, not on its own. */
    if (!bluetooth_is_powered()) {
        gfx_blit_1bpp(MARGIN, 0, ICON_ANTENNA_W, ICON_ANTENNA_H,
                      icon_antenna, gfx_rgb(96, 96, 96));
    } else if (bluetooth_get_conn_state() == BLUETOOTH_CONN_CONNECTED) {
        gfx_blit_1bpp(MARGIN, 0, ICON_ANTENNA_ON_W, ICON_ANTENNA_ON_H,
                      icon_antenna_on, GFX_WHITE);
    } else {
        gfx_blit_1bpp(MARGIN, 0, ICON_ANTENNA_W, ICON_ANTENNA_H,
                      icon_antenna, GFX_WHITE);
    }

    /* Centre: the active audio output. White "JACK" for the wired path, Bluetooth-blue
       "BT" for the speaker; dimmed when muted (VOLUME_OUT_NONE, the wired path silenced). */
    const char *out_txt;
    gfx_color_t out_col;
    switch (volume_get_output()) {
    case VOLUME_OUT_BT:
        out_txt = "BT";   out_col = gfx_rgb(0, 130, 252); break;
    case VOLUME_OUT_DAC:
        out_txt = "JACK"; out_col = GFX_WHITE;            break;
    default: /* VOLUME_OUT_NONE */
        out_txt = "JACK"; out_col = gfx_rgb(96, 96, 96);  break;
    }
    int ow = (int)strlen(out_txt) * GFX_CHAR_W;
    int oy = (STATUS_BAR_H - GFX_CHAR_H) / 2;
    gfx_draw_text((GFX_W - ow) / 2, oy, out_txt, out_col, 1);

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
