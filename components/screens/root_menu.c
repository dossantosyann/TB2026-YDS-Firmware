#include "root_menu.h"
#include "gfx.h"
#include "icons.h"

/* Home layout: a centered horizontal row of tiles on a black background. Each
   tile is framed by 2px white corner brackets (not a full border) around a black
   interior, with the icon pattern blitted in white inside it. The brackets avoid
   long solid horizontal lines, which light up OFF pixels across the whole row on
   this passive OLED (IR drop along the common line). Keeping the background and
   interiors black also minimizes lit pixels. */
#define ICON_SZ  32                 /* all menu icons are 32x32 */
#define BORDER   2                  /* bracket thickness in pixels */
#define TILE     (ICON_SZ + 2 * BORDER)  /* 36x36: 32px icon + frame */
#define BRACKET  8                  /* corner arm length in pixels */
#define GAP      12                 /* space between tiles */
#define N_TILES  3

/* One icon per tile, left to right. All menu icons are 32x32 (see icons.h). */
static const uint8_t *const s_icons[N_TILES] = {
    icon_music,
    icon_stats,
    icon_settings1,
};

static void on_enter(screen_t *self)                    { (void)self; }
static void on_exit(screen_t *self)                     { (void)self; }
static void handle_input(screen_t *self, ui_event_t ev) { (void)self; (void)ev; }

static void render(screen_t *self)
{
    (void)self;
    gfx_clear(GFX_BLACK);

    int row_w = N_TILES * TILE + (N_TILES - 1) * GAP;
    int x0    = (GFX_W - row_w) / 2;   /* center the row horizontally */
    int y     = (GFX_H - TILE) / 2;    /* center it vertically        */

    for (int i = 0; i < N_TILES; i++) {
        int x = x0 + i * (TILE + GAP);
        int r = x + TILE;   /* one past the right edge  */
        int b = y + TILE;   /* one past the bottom edge */
        /* 2px white corner brackets; the interior stays black from gfx_clear. */
        gfx_fill_rect(x,           y,           BRACKET, BORDER,  GFX_WHITE);  /* top-left  */
        gfx_fill_rect(x,           y,           BORDER,  BRACKET, GFX_WHITE);
        gfx_fill_rect(r - BRACKET, y,           BRACKET, BORDER,  GFX_WHITE);  /* top-right */
        gfx_fill_rect(r - BORDER,  y,           BORDER,  BRACKET, GFX_WHITE);
        gfx_fill_rect(x,           b - BORDER,  BRACKET, BORDER,  GFX_WHITE);  /* bot-left  */
        gfx_fill_rect(x,           b - BRACKET, BORDER,  BRACKET, GFX_WHITE);
        gfx_fill_rect(r - BRACKET, b - BORDER,  BRACKET, BORDER,  GFX_WHITE);  /* bot-right */
        gfx_fill_rect(r - BORDER,  b - BRACKET, BORDER,  BRACKET, GFX_WHITE);
        /* Icon pattern in white, inside the brackets. */
        gfx_blit_1bpp(x + BORDER, y + BORDER, ICON_SZ, ICON_SZ, s_icons[i], GFX_WHITE);
    }
}

screen_t *root_menu(void)
{
    static screen_t s = {
        .on_enter     = on_enter,
        .on_exit      = on_exit,
        .handle_input = handle_input,
        .render       = render,
    };
    return &s;
}
