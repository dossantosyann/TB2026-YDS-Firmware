/**
 * @file duration_picker.c
 * @brief Preset-list duration picker (see duration_picker.h).
 */
#include "duration_picker.h"
#include "navigator.h"
#include "status_bar.h"
#include "gfx.h"

#include <stdio.h>

#define ACCENT      gfx_rgb(255, 0, 0)       /* Settings identity colour */
#define DIM         gfx_rgb(110, 110, 110)

#define HDR_Y       (STATUS_BAR_H + 8)
#define LIST_Y0     (STATUS_BAR_H + 34)
#define ROW_H       22
#define ARROW_X     6
#define TEXT_X      22

/* Human-readable duration into buf: "Never" (0), "30 s" (<1 min), else whole minutes. */
static void fmt_duration(uint32_t ms, char *buf, size_t n)
{
    if (ms == 0)             snprintf(buf, n, "Never");
    else if (ms < 60000u)    snprintf(buf, n, "%u s", (unsigned)(ms / 1000u));
    else                     snprintf(buf, n, "%u min", (unsigned)(ms / 60000u));
}

static void render(screen_t *self)
{
    duration_picker_t *dp = (duration_picker_t *)self;
    gfx_clear(GFX_BLACK);

    gfx_draw_text(4, HDR_Y, ">", ACCENT, 1);
    gfx_draw_text(16, HDR_Y, dp->title, DIM, 1);

    for (int i = 0; i < dp->count; i++) {
        int y = LIST_Y0 + i * ROW_H;
        bool sel = (i == dp->sel);
        char buf[12];
        fmt_duration(dp->presets[i], buf, sizeof buf);
        if (sel)
            gfx_draw_text(ARROW_X, y, ">", ACCENT, 1);
        gfx_draw_text(TEXT_X, y, buf, sel ? GFX_WHITE : DIM, 1);
    }
}

static void handle_input(screen_t *self, ui_event_t ev)
{
    duration_picker_t *dp = (duration_picker_t *)self;
    switch (ev) {
    case UI_EVENT_UP:   if (dp->sel > 0)              dp->sel--; break;
    case UI_EVENT_DOWN: if (dp->sel < dp->count - 1)  dp->sel++; break;
    case UI_EVENT_BACK:
        if (dp->presets[dp->sel] != dp->get())   /* persist once, only if it moved */
            dp->set(dp->presets[dp->sel]);
        navigator_pop();
        break;
    default: break;
    }
}

/* Seed the highlight from the stored value; fall back to the first row if it isn't a
   preset (should not happen — every persisted value comes from this list). */
static void on_enter(screen_t *self)
{
    duration_picker_t *dp = (duration_picker_t *)self;
    uint32_t cur = dp->get();
    dp->sel = 0;
    for (int i = 0; i < dp->count; i++) {
        if (dp->presets[i] == cur) { dp->sel = i; break; }
    }
}

static void noop(screen_t *self) { (void)self; }

void duration_picker_init(duration_picker_t *dp, const char *title,
                          const uint32_t *presets, int count,
                          uint32_t (*get)(void), void (*set)(uint32_t ms))
{
    dp->base.on_enter     = on_enter;
    dp->base.on_exit      = noop;
    dp->base.handle_input = handle_input;
    dp->base.render       = render;
    dp->base.refresh_ms   = 0;
    dp->title   = title;
    dp->presets = presets;
    dp->count   = count;
    dp->sel     = 0;
    dp->get     = get;
    dp->set     = set;
}
