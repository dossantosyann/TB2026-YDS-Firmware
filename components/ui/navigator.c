/**
 * @file navigator.c
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
// Screen stack router

#include "navigator.h"
#include "status_bar.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

void navigator_render(void)
{
    if (s_depth == 0) return;
    screen_t *top = s_stack[s_depth - 1];
    if (top->render) top->render(top);
    status_bar_draw();   /* battery overlay, on top of every screen */
}

void navigator_tick(ui_event_t event)
{
    if (s_depth == 0) return;
    screen_t *top = s_stack[s_depth - 1];
    if (top->handle_input) top->handle_input(top, event);
    /* handle_input may have pushed or popped, so re-read the top and render
       whatever is now on screen -- otherwise a push/pop would only take effect
       on the next event (the "press twice to enter/leave" bug). */
    navigator_render();
}

uint32_t navigator_refresh_ticks(void)
{
    if (s_depth == 0) return portMAX_DELAY;
    unsigned ms = s_stack[s_depth - 1]->refresh_ms;
    return ms ? pdMS_TO_TICKS(ms) : portMAX_DELAY;
}
