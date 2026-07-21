/* Renders every UI screen off-target and dumps each to its own .ppm, for the LaTeX-report
   screenshot export (see screenshot.sh). No ESP-IDF, no hardware: real gfx.c/navigator.c/icons.c
   + every real screen .c, with every service/driver call answered by fakes/screens/service_stubs.c
   (canned data -- see that file for the fixture library/battery/Bluetooth state shown). */
#include <stdio.h>
#include <stdint.h>

#include "gfx.h"
#include "screen.h"
#include "status_bar.h"
#include "navigator.h"

#include "root_menu.h"
#include "now_playing.h"
#include "output_select.h"
#include "playlist_browser.h"
#include "storage_screen.h"
#include "stats_screen.h"
#include "battery_test.h"
#include "fuel_gauge_debug.h"
#include "settings_screen.h"
#include "bluetooth_settings.h"
#include "audio_settings.h"
#include "screen_settings.h"
#include "power_settings.h"

static void dump(const char *name)
{
    char path[256];
    snprintf(path, sizeof path, "test/host/screenshots/%s.ppm", name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P6\n%d %d\n255\n", GFX_W, GFX_H);
    for (int i = 0; i < GFX_W * GFX_H; i++) {
        gfx_color_t c = gfx_buffer()[i];
        fputc(((c >> 11) & 0x1F) * 255 / 31, f);
        fputc(((c >> 5)  & 0x3F) * 255 / 63, f);
        fputc(( c        & 0x1F) * 255 / 31, f);
    }
    fclose(f);
    printf("wrote %s\n", path);
}

/* Render with no lifecycle call: matches how root_menu_test/menu_screen_test already do it.
   status_bar_draw() is called after render(), same order as navigator_render() -- the
   navigator itself is never involved here, so this tool has to redo that one step. */
static void shot(const char *name, screen_t *s)
{
    s->render(s);
    status_bar_draw();
    dump(name);
}

/* Render after on_enter(): screens that seed their state there (cursor, scroll, popup flags). */
static void shot_entered(const char *name, screen_t *s)
{
    if (s->on_enter) s->on_enter(s);
    s->render(s);
    status_bar_draw();
    dump(name);
}

/* The five stats detail pages (Battery/Storage/Inputs/Peripherals/System) are static screen_t
   instances private to stats_screen.c -- no constructor exposes them, so the only way to reach
   one is to actually navigate there, same as the real UI: from the stats menu (already pushed,
   selection persists across calls), press DOWN `downs` times then SELECT to enter the page. */
static void stats_detail(const char *name, int downs)
{
    for (int i = 0; i < downs; i++) navigator_tick(UI_EVENT_DOWN);
    navigator_tick(UI_EVENT_SELECT);   /* pushes the detail screen, renders it + the status bar */
    dump(name);
    navigator_pop();                   /* back to the stats menu, ready for the next item */
}

int main(void)
{
    shot("root_menu", root_menu());

    shot_entered("now_playing", now_playing_screen());
    shot_entered("output_select", output_select_screen());
    shot_entered("playlist_browser", playlist_browser_screen());
    shot_entered("storage_screen", storage_screen());

    shot("stats_menu", stats_screen());
    navigator_push(stats_screen());
    stats_detail("stats_battery",     0);
    stats_detail("stats_storage",     1);
    stats_detail("stats_inputs",      1);
    stats_detail("stats_peripherals", 1);
    stats_detail("stats_system",      1);
    navigator_pop();
    shot_entered("stats_autonomy_test", battery_test_screen());
    shot_entered("stats_battery_fuel_gauge_debug", fuel_gauge_debug_screen());

    shot("settings_menu", settings_screen());
    shot_entered("bluetooth_settings", bluetooth_settings_screen());
    shot_entered("audio_settings", audio_settings_screen());
    screen_settings_restore();   /* seeds brightness from (fake, empty) NVS before on_enter */
    shot_entered("screen_settings", screen_settings_screen());
    shot_entered("power_settings", power_settings_screen());

    printf("done: 18 screenshots written to test/host/screenshots/\n");
    return 0;
}
