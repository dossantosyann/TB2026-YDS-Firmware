/* Host unit test for the navigator screen-stack router.
   Verifies push/pop/tick dispatch semantics. No ESP-IDF, no hardware. */
#include "navigator.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    screen_t   base;
    int        enter, exited, render, input;
    ui_event_t last_event;
} fake_t;

static void f_enter(screen_t *s)               { ((fake_t *)s)->enter++; }
static void f_exit(screen_t *s)                { ((fake_t *)s)->exited++; }
static void f_render(screen_t *s)              { ((fake_t *)s)->render++; }
static void f_input(screen_t *s, ui_event_t e) { fake_t *f = (fake_t *)s; f->input++; f->last_event = e; }

static fake_t make_fake(void)
{
    fake_t f;
    memset(&f, 0, sizeof f);
    f.base.on_enter     = f_enter;
    f.base.on_exit      = f_exit;
    f.base.handle_input = f_input;
    f.base.render       = f_render;
    return f;
}

int main(void)
{
    fake_t a = make_fake(), b = make_fake();

    /* push enters the pushed screen */
    navigator_push(&a.base);
    assert(a.enter == 1);

    /* tick routes input+render to the TOP screen only, never the one below */
    navigator_push(&b.base);
    assert(b.enter == 1);
    navigator_tick(UI_EVENT_SELECT);
    assert(b.input == 1 && b.last_event == UI_EVENT_SELECT && b.render == 1);
    assert(a.input == 0 && a.render == 0);

    /* pop exits the top, and the screen below becomes active again */
    navigator_pop();
    assert(b.exited == 1);
    navigator_tick(UI_EVENT_UP);
    assert(a.input == 1 && a.last_event == UI_EVENT_UP && a.render == 1);
    assert(b.input == 1);   /* popped screen no longer ticked */

    /* unwind to empty; an extra pop and a tick on empty must be safe no-ops */
    navigator_pop();
    assert(a.exited == 1);
    navigator_pop();                 /* already empty */
    navigator_tick(UI_EVENT_DOWN);   /* empty: nothing happens */
    assert(a.input == 1 && a.render == 1);

    /* stack is bounded (NAV_STACK_MAX = 8): the 9th push is rejected */
    fake_t s[9];
    for (int i = 0; i < 9; i++) s[i] = make_fake();
    for (int i = 0; i < 9; i++) navigator_push(&s[i].base);
    assert(s[7].enter == 1);   /* 8th accepted */
    assert(s[8].enter == 0);   /* 9th rejected, no overflow */

    printf("navigator: all assertions passed\n");
    return 0;
}
