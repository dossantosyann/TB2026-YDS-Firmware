#include "storage_screen.h"
#include "navigator.h"
#include "gfx.h"

/* Stub: a black page with a title; BACK returns home. Real content lands later. */
static void on_enter(screen_t *self) { (void)self; }
static void on_exit(screen_t *self)  { (void)self; }

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    if (ev == UI_EVENT_BACK) navigator_pop();
}

static void render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);
    gfx_draw_text(8, 16, "Storage", GFX_WHITE, 2);
}

screen_t *storage_screen(void)
{
    static screen_t s = {
        .on_enter     = on_enter,
        .on_exit      = on_exit,
        .handle_input = handle_input,
        .render       = render,
    };
    return &s;
}
