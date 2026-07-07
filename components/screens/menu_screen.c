#include "menu_screen.h"
#include "navigator.h"
#include "gfx.h"

static void on_enter(screen_t *self) { (void)self; }
static void on_exit(screen_t *self)  { (void)self; }

static void handle_input(screen_t *self, ui_event_t event)
{
    menu_screen_t *ms = (menu_screen_t *)self;
    if (event == UI_EVENT_UP && ms->item_count > 0)
        ms->selected = (ms->selected - 1 + ms->item_count) % ms->item_count;
    else if (event == UI_EVENT_DOWN && ms->item_count > 0)
        ms->selected = (ms->selected + 1) % ms->item_count;
    else if (event == UI_EVENT_SELECT) {
        const menu_item_t *item = &ms->items[ms->selected];
        if (item->navigate_to) navigator_push(item->navigate_to);
        else if (item->action)  item->action();
    } else if (event == UI_EVENT_BACK) {
        navigator_pop();
    }
}

/* List layout: 22px rows (8 fit the 176px panel); the selected row is an inverted bar. */
#define MENU_ROW_H  22
#define MENU_TEXT_X 8
#define MENU_SCALE  2

/* Draws the menu into the gfx framebuffer. Presenting it (gfx_flush) is left to the
   navigator/UI task, so a full-screen SPI blit isn't forced on every tick. */
static void render(screen_t *self)
{
    menu_screen_t *ms = (menu_screen_t *)self;
    gfx_clear(GFX_BLACK);
    for (int i = 0; i < ms->item_count; i++) {
        int y = i * MENU_ROW_H;
        gfx_color_t fg = GFX_WHITE;
        if (i == ms->selected) {
            gfx_fill_rect(0, y, GFX_W, MENU_ROW_H, GFX_WHITE);   /* selection bar */
            fg = GFX_BLACK;
        }
        int ty = y + (MENU_ROW_H - GFX_CHAR_H * MENU_SCALE) / 2; /* vertically center the glyph */
        gfx_draw_text(MENU_TEXT_X, ty, ms->items[i].label, fg, MENU_SCALE);
    }
}

void menu_screen_init(menu_screen_t *ms, const menu_item_t *items, int count)
{
    ms->base.on_enter     = on_enter;
    ms->base.on_exit      = on_exit;
    ms->base.handle_input = handle_input;
    ms->base.render       = render;
    ms->items      = items;
    ms->item_count = count;
    ms->selected   = 0;
}
