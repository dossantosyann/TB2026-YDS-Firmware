#include "navigator.h"

#define NAV_STACK_MAX 8

static screen_t *s_stack[NAV_STACK_MAX];
static int       s_depth = 0;

void navigator_push(screen_t *screen)
{
    if (s_depth >= NAV_STACK_MAX) return;
    s_stack[s_depth++] = screen;
    if (screen->on_enter) screen->on_enter(screen);
}

void navigator_pop(void)
{
    if (s_depth == 0) return;
    screen_t *top = s_stack[--s_depth];
    if (top->on_exit) top->on_exit(top);
}

void navigator_tick(ui_event_t event)
{
    if (s_depth == 0) return;
    screen_t *top = s_stack[s_depth - 1];
    if (top->handle_input) top->handle_input(top, event);
    if (top->render)       top->render(top);
}
