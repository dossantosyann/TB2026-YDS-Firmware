#include "menu_screen.h"
#include "navigator.h"

static void on_enter(screen_t *self) { (void)self; }
static void on_exit(screen_t *self)  { (void)self; }

static void handle_input(screen_t *self, ui_event_t event)
{
    menu_screen_t *ms = (menu_screen_t *)self;
    if (event == UI_EVENT_UP && ms->selected > 0)
        ms->selected--;
    else if (event == UI_EVENT_DOWN && ms->selected < ms->item_count - 1)
        ms->selected++;
    else if (event == UI_EVENT_SELECT) {
        const menu_item_t *item = &ms->items[ms->selected];
        if (item->navigate_to) navigator_push(item->navigate_to);
        else if (item->action)  item->action();
    } else if (event == UI_EVENT_BACK) {
        navigator_pop();
    }
}

static void render(screen_t *self) { (void)self; }

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
