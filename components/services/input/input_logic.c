/**
 * @file input_logic.c
 * @brief Pure debounce + press-edge detection + button->ui_event mapping (no hardware).
 * @ingroup services_input_logic
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "input_logic.h"
#include "board_pins.h"

/* A is not on the expander; this sentinel marks "use a_pressed instead of an expander bit". */
#define BTN_A_NOT_ON_EXPANDER 0xFF

/* Button index (0..INPUT_BTN_COUNT-1) -> (expander channel, emitted event). The expander
   channels come from board_pins.h so this stays in sync with the wiring; A maps to SELECT and
   is sampled from its own GPIO, not the expander port. */
static const struct {
    uint8_t    exp_bit;   /* EXP_BTN_* channel, or BTN_A_NOT_ON_EXPANDER for A */
    ui_event_t ev;
} BTN[INPUT_BTN_COUNT] = {
    { EXP_BTN_UP,    UI_EVENT_UP     },
    { EXP_BTN_DOWN,  UI_EVENT_DOWN   },
    { EXP_BTN_LEFT,  UI_EVENT_LEFT   },
    { EXP_BTN_RIGHT, UI_EVENT_RIGHT  },
    { EXP_BTN_B,     UI_EVENT_BACK   },
    { BTN_A_NOT_ON_EXPANDER, UI_EVENT_SELECT },
};

int input_logic_update(input_debounce_t *st, uint8_t exp_port, bool a_pressed,
                       uint32_t now_ms, ui_event_t *out, int out_cap)
{
    /* Collapse both sources into one pressed mask in button-index space. Expander buttons are
       active-low (pressed = bit 0); IO7/INOKB is never in BTN[], so it can never set a bit. */
    uint8_t pressed = 0;
    for (int i = 0; i < INPUT_BTN_COUNT; i++) {
        bool down = (BTN[i].exp_bit == BTN_A_NOT_ON_EXPANDER)
                        ? a_pressed
                        : ((exp_port >> BTN[i].exp_bit) & 1) == 0;
        if (down) pressed |= (uint8_t)(1u << i);
    }

    int n = 0;
    for (int i = 0; i < INPUT_BTN_COUNT; i++) {
        uint8_t bit = (uint8_t)(1u << i);
        bool now = pressed & bit;
        bool was = st->prev & bit;
        /* Press edge only; the lockout swallows bounce and never fires on hold or release. */
        if (now && !was && (uint32_t)(now_ms - st->last_ms[i]) >= INPUT_DEBOUNCE_MS) {
            if (n < out_cap) out[n++] = BTN[i].ev;
            st->last_ms[i] = now_ms;
            st->count[i]++;
        }
    }
    st->prev = pressed;
    return n;
}
