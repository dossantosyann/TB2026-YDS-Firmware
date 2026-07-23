/* Host unit test for the pure input logic (input_logic.c).
   Feeds sequences of (expander port byte, A level, timestamp) and asserts the produced
   ui_event_t stream: one event per press, bounce filtered, holds/releases silent, the
   charger INOKB (IO7) ignored, and the button->event mapping. No ESP-IDF, no hardware. */
#include "input_logic.h"
#include "board_pins.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Expander input port at rest: buttons (IO2..IO6) released = high, INOKB (IO7) low,
   outputs (IO0/IO1) low. Verified on the board (memory hardware-bringup). */
#define REST       0x7C
#define PRESS(bit) (REST & ~(1u << (bit)))   /* press one expander button: clear its bit */

static input_debounce_t st;

/* Run one sample; return the event count, and the first event via *first (if any). */
static int step(uint8_t port, bool a, uint32_t t, ui_event_t *first)
{
    ui_event_t out[INPUT_BTN_COUNT];
    int n = input_logic_update(&st, port, a, t, out, INPUT_BTN_COUNT);
    if (n > 0 && first) *first = out[0];
    return n;
}

/* Each expander button: a press emits exactly its event once; the release emits nothing. */
static void test_each_expander_button(void)
{
    const struct { uint8_t bit; ui_event_t ev; const char *name; } cases[] = {
        { EXP_BTN_UP,    UI_EVENT_UP,    "UP"    },
        { EXP_BTN_DOWN,  UI_EVENT_DOWN,  "DOWN"  },
        { EXP_BTN_LEFT,  UI_EVENT_LEFT,  "LEFT"  },
        { EXP_BTN_RIGHT, UI_EVENT_RIGHT, "RIGHT" },
        { EXP_BTN_B,     UI_EVENT_BACK,  "B"     },
    };
    uint32_t t = 1000;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        memset(&st, 0, sizeof st);
        ui_event_t ev = (ui_event_t)-1;

        assert(step(PRESS(cases[i].bit), false, t, &ev) == 1);   /* press -> one event */
        assert(ev == cases[i].ev);                               /* correct mapping */
        assert(step(REST, false, t + 5, NULL) == 0);             /* release -> nothing */
    }
    printf("input: each expander button -> one mapped event, release silent OK\n");
}

/* Button A is on its own GPIO (active-low): pressed -> SELECT once, release silent. */
static void test_button_a(void)
{
    memset(&st, 0, sizeof st);
    ui_event_t ev = (ui_event_t)-1;

    assert(step(REST, true, 1000, &ev) == 1);   /* A pressed (GPIO39 low) */
    assert(ev == UI_EVENT_SELECT);
    assert(step(REST, true, 1100, NULL) == 0);  /* still held -> nothing */
    assert(step(REST, false, 1200, NULL) == 0); /* released   -> nothing */
    printf("input: A -> single SELECT, hold/release silent OK\n");
}

/* Contact bounce shorter than the lockout collapses to a single event; a genuine press
   after the lockout is accepted again (the lockout must not wedge). */
static void test_bounce(void)
{
    memset(&st, 0, sizeof st);

    assert(step(PRESS(EXP_BTN_UP), false, 100, NULL) == 1);  /* first edge accepted */
    assert(step(REST,             false, 105, NULL) == 0);
    assert(step(PRESS(EXP_BTN_UP), false, 110, NULL) == 0);  /* re-press +10ms < 30ms -> swallowed */
    assert(step(REST,             false, 115, NULL) == 0);
    assert(step(PRESS(EXP_BTN_UP), false, 200, NULL) == 1);  /* +100ms > 30ms -> new press accepted */
    printf("input: bounce < %dms filtered, real re-press after lockout accepted OK\n",
           INPUT_DEBOUNCE_MS);
}

/* Holding a button down emits nothing after the initial press. */
static void test_hold(void)
{
    memset(&st, 0, sizeof st);

    assert(step(PRESS(EXP_BTN_DOWN), false, 1000, NULL) == 1);
    assert(step(PRESS(EXP_BTN_DOWN), false, 2000, NULL) == 0);  /* held */
    assert(step(PRESS(EXP_BTN_DOWN), false, 3000, NULL) == 0);  /* still held */
    assert(step(REST,               false, 4000, NULL) == 0);   /* released */
    printf("input: hold -> no repeat, release silent OK\n");
}

/* The fuel-gauge INOKB (IO7) is not a button: toggling it emits nothing. */
static void test_inokb_ignored(void)
{
    memset(&st, 0, sizeof st);

    assert(step(REST | (1u << EXP_INOKB), false, 1000, NULL) == 0);  /* INOKB asserted */
    assert(step(REST,                     false, 1100, NULL) == 0);  /* INOKB cleared  */
    printf("input: INOKB (IO7) toggle -> no event OK\n");
}

int main(void)
{
    test_each_expander_button();
    test_button_a();
    test_bounce();
    test_hold();
    test_inokb_ignored();
    printf("input: all assertions passed\n");
    return 0;
}
