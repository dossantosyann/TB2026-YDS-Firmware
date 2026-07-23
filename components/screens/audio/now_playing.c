/**
 * @file now_playing.c
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "now_playing.h"
#include "playlist_browser.h"
#include "output_select.h"
#include "navigator.h"
#include "gfx.h"
#include "icons.h"
#include "status_bar.h"
#include "player.h"
#include "playlist.h"
#include "track_meta.h"
#include "volume.h"
#include "adc.h"
#include <string.h>
#include <stdio.h>

/* ---- layout ------------------------------------------------------------ */
/* Content area: x=0..151 (152 px). Right strip x=152..175 = volume column. */
#define CONT_W      152
#define CONT_CX     76

/* Nav hint icon+text centering: icon (8px) + 2px gap + label text at scale 1. */
#define NAV_ICON_GAP  2
#define NAV_TOP_IX    ((CONT_W - (ICON_ARROW_UP_W   + NAV_ICON_GAP + 16 * GFX_CHAR_W)) / 2)
#define NAV_BOT_IX    ((CONT_W - (ICON_ARROW_DOWN_W + NAV_ICON_GAP +  8 * GFX_CHAR_W)) / 2)

/* Vertical positions */
#define NAV_TOP_Y   (STATUS_BAR_H + 2)
#define TITLE_Y     (STATUS_BAR_H + 30)
#define ARTIST_Y    (TITLE_Y + 20)
#define ALBUM_Y     (ARTIST_Y + 10)
#define QUALITY_Y   (ALBUM_Y + 10)
#define CTRL_Y      (ALBUM_Y + 30)
#define PROG_ROW_Y  (CTRL_Y + 22)
#define TIME_Y      (PROG_ROW_Y + 18)
#define NAV_BOT_Y   164

/* Transport icon x positions (all icons are 16×16) */
#define CTRL_PREV_X (CONT_CX - 40)
#define CTRL_PLAY_X (CONT_CX - 8)
#define CTRL_NEXT_X (CONT_CX + 24)

/* Progress bar: 4px margin | loop icon | 2px | bar | 2px | shuffle icon | 4px margin */
#define PROG_MARGIN  4
#define PROG_LOOP_X  PROG_MARGIN
#define PROG_BAR_X   (PROG_LOOP_X + ICON_LOOP_W + 2)
#define PROG_BAR_W   108
#define PROG_BAR_Y   (PROG_ROW_Y + 6)
#define PROG_BAR_H   4
#define SHUF_ICON_X  (PROG_BAR_X + PROG_BAR_W + 2)

/* Volume column (right strip) */
#define VOL_ICON_X  156
#define VOL_ICON_Y  (STATUS_BAR_H + 4)
#define VOL_BAR_X   160
#define VOL_BAR_Y   (VOL_ICON_Y + 20)
#define VOL_BAR_W   8
#define VOL_BAR_H   (NAV_BOT_Y - VOL_BAR_Y)

/* Scrollable text clip zone: left edge of loop icon → right edge of shuffle icon */
#define TEXT_X0     PROG_LOOP_X                        /* 4   */
#define TEXT_X1     (SHUF_ICON_X + ICON_SHUFFLE_W)     /* 148 */
#define TEXT_MAX_W  (TEXT_X1 - TEXT_X0)                /* 144 */

/* 8 px per 250 ms tick ≈ 32 px/s; 2-tick (0.5 s) pause at each end */
#define SCROLL_SPEED  8
#define SCROLL_PAUSE  2

#define CACHED_PATH_MAX 128

/* ---- ctrl cursor ------------------------------------------------------- */
/* 0=loop, 1=prev, 2=play/pause, 3=next, 4=shuffle */
typedef struct { int x, y; } ctrl_xy_t;
static const ctrl_xy_t k_ctrl[5] = {
    {PROG_LOOP_X, PROG_ROW_Y},
    {CTRL_PREV_X, CTRL_Y},
    {CTRL_PLAY_X, CTRL_Y},
    {CTRL_NEXT_X, CTRL_Y},
    {SHUF_ICON_X, PROG_ROW_Y},
};

/* ---- state ------------------------------------------------------------- */
typedef struct { int32_t offset; int32_t pause; } scroll_t;

static track_meta_t  s_meta;
static char          s_cached_path[CACHED_PATH_MAX];
static scroll_t      s_title_sc, s_artist_sc, s_album_sc;
static int           s_cursor;

/* ---- helpers ----------------------------------------------------------- */
/* Draws 4 corner brackets (arm=3px) around the rect (x,y,w,h). */
static void draw_corners(int x, int y, int w, int h, gfx_color_t col)
{
    int arm = 3;
    gfx_fill_rect(x,         y,             arm, 1,   col);
    gfx_fill_rect(x,         y,             1,   arm,  col);
    gfx_fill_rect(x + w - arm, y,           arm, 1,   col);
    gfx_fill_rect(x + w - 1,   y,           1,   arm,  col);
    gfx_fill_rect(x,           y + h - 1,   arm, 1,   col);
    gfx_fill_rect(x,           y + h - arm, 1,   arm,  col);
    gfx_fill_rect(x + w - arm, y + h - 1,   arm, 1,   col);
    gfx_fill_rect(x + w - 1,   y + h - arm, 1,   arm,  col);
}

static void reset_scroll(void)
{
    s_title_sc.offset  = 0; s_title_sc.pause  = 0;
    s_artist_sc.offset = 0; s_artist_sc.pause = 0;
    s_album_sc.offset  = 0; s_album_sc.pause  = 0;
}

/* Draw text clipped to [TEXT_X0, TEXT_X1), scrolling if it overflows.
   Short strings are centered; scroll state is advanced on each call. */
static void draw_scrolled(int y, const char *text, int scale, scroll_t *sc, gfx_color_t color)
{
    int text_px = (int)strlen(text) * GFX_CHAR_W * scale;
    int h       = GFX_CHAR_H * scale;

    if (text_px <= TEXT_MAX_W) {
        sc->offset = 0; sc->pause = 0;
        gfx_draw_text(TEXT_X0 + (TEXT_MAX_W - text_px) / 2, y, text, color, scale);
        return;
    }

    /* draw with scroll offset; gfx clips x<0 on the left automatically */
    gfx_draw_text(TEXT_X0 - sc->offset, y, text, color, scale);
    /* black out areas outside the clip zone */
    gfx_fill_rect(0,      y, TEXT_X0,         h, GFX_BLACK);
    gfx_fill_rect(TEXT_X1, y, GFX_W - TEXT_X1, h, GFX_BLACK);

    int32_t max_off  = text_px - TEXT_MAX_W;
    bool    at_start = sc->offset <= 0;
    bool    at_end   = sc->offset >= max_off;

    if ((at_start || at_end) && sc->pause < SCROLL_PAUSE) {
        sc->pause++;                /* hold at each end so the tail is readable */
    } else if (at_end) {
        sc->offset = 0;             /* wrap back to the start */
        sc->pause  = 0;
    } else {
        sc->offset += SCROLL_SPEED;
        if (sc->offset > max_off) sc->offset = max_off;  /* clamp so the end shows */
        sc->pause = 0;
    }
}

static void format_time(char *buf, size_t n, uint32_t ms)
{
    uint32_t s = ms / 1000;
    snprintf(buf, n, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

/* ---- screen ------------------------------------------------------------ */
static void np_enter(screen_t *self)
{
    (void)self;
    s_cached_path[0] = '\0';
    reset_scroll();
    s_cursor = 2;
}

static void np_exit(screen_t *self) { (void)self; }

static void handle_select(void)
{
    player_status_t status = {0};
    player_get_state(&status);

    switch (s_cursor) {
    case 0: {
        playlist_repeat_t r = playlist_get_repeat();
        if      (r == PLAYLIST_REPEAT_OFF) playlist_set_repeat(PLAYLIST_REPEAT_ALL);
        else if (r == PLAYLIST_REPEAT_ALL) playlist_set_repeat(PLAYLIST_REPEAT_ONE);
        else                               playlist_set_repeat(PLAYLIST_REPEAT_OFF);
        break;
    }
    case 1: player_prev(); break;
    case 2:
        if      (status.state == PLAYER_PLAYING) player_pause();
        else if (status.state == PLAYER_PAUSED)  player_resume();
        else                                     player_start();
        break;
    case 3: player_next(); break;
    case 4: playlist_set_shuffle(!playlist_get_shuffle()); break;
    }
}

static void handle_input(screen_t *self, ui_event_t ev)
{
    (void)self;
    switch (ev) {
    case UI_EVENT_BACK:   navigator_pop();                        break;
    case UI_EVENT_LEFT:   s_cursor = (s_cursor - 1 + 5) % 5;    break;
    case UI_EVENT_RIGHT:  s_cursor = (s_cursor + 1) % 5;         break;
    case UI_EVENT_SELECT: handle_select();                        break;
    case UI_EVENT_UP:     navigator_push(output_select_screen());   break;
    case UI_EVENT_DOWN:   navigator_push(playlist_browser_screen());  break;
    default: break;
    }
}

static void render(screen_t *self)
{
    (void)self;

    player_status_t status = {0};
    player_get_state(&status);

    /* refresh metadata on track change. When stopped, fall back to the playlist's
       current track (e.g. re-selected at boot after an auto power-off): it is what
       SELECT on play would start. */
    const char *path = NULL, *name = NULL;
    if (status.state != PLAYER_STOPPED) {
        path = status.track.path;
        name = status.track.name;
    } else {
        playlist_track_t cur;
        if (playlist_current(&cur) == ESP_OK) {
            path = cur.path;
            name = cur.name;
        }
    }
    if (path && strncmp(s_cached_path, path, CACHED_PATH_MAX) != 0) {
        strncpy(s_cached_path, path, CACHED_PATH_MAX - 1);
        s_cached_path[CACHED_PATH_MAX - 1] = '\0';
        memset(&s_meta, 0, sizeof s_meta);
        track_meta_read(path, &s_meta);
        reset_scroll();
    } else if (!path) {
        memset(&s_meta, 0, sizeof s_meta);
        s_cached_path[0] = '\0';
    }

    gfx_clear(GFX_BLACK);

    /* navigation hints */
    gfx_blit_1bpp(NAV_TOP_IX, NAV_TOP_Y, ICON_ARROW_UP_W, ICON_ARROW_UP_H, icon_arrow_up, GFX_WHITE);
    gfx_draw_text(NAV_TOP_IX + ICON_ARROW_UP_W + NAV_ICON_GAP, NAV_TOP_Y, "OUTPUT SELECTION", GFX_WHITE, 1);
    gfx_blit_1bpp(NAV_BOT_IX, NAV_BOT_Y, ICON_ARROW_DOWN_W, ICON_ARROW_DOWN_H, icon_arrow_down, GFX_WHITE);
    gfx_draw_text(NAV_BOT_IX + ICON_ARROW_DOWN_W + NAV_ICON_GAP, NAV_BOT_Y, "PLAYLIST", GFX_WHITE, 1);

    /* track metadata */
    const char *title  = s_meta.title[0]  ? s_meta.title
                       : (path ? name : "---");
    const char *artist = s_meta.artist;
    const char *album  = s_meta.album;

    draw_scrolled(TITLE_Y,  title,  2, &s_title_sc,  GFX_WHITE);
    draw_scrolled(ARTIST_Y, artist, 1, &s_artist_sc, GFX_WHITE);
    draw_scrolled(ALBUM_Y,  album,  1, &s_album_sc,  GFX_WHITE);

    if (status.track_unsupported) {
        /* The output refused the track's format (non-44.1 kHz over Bluetooth): explain the
           stop. Drawn in the quality line's spot, which is empty when nothing is playing. */
        const char *m1 = "Track not playable";
        const char *m2 = "BT: 44.1 kHz only";
        gfx_draw_text((CONT_W - (int)strlen(m1) * GFX_CHAR_W) / 2, QUALITY_Y,
                      m1, gfx_rgb(255, 170, 0), 1);
        gfx_draw_text((CONT_W - (int)strlen(m2) * GFX_CHAR_W) / 2, QUALITY_Y + 10,
                      m2, gfx_rgb(255, 170, 0), 1);
    } else if (s_meta.rate_hz > 0) {
        char q[24];
        snprintf(q, sizeof q, "%lu Hz / %u-bit",
                 (unsigned long)s_meta.rate_hz, s_meta.bits);
        int qx = (CONT_W - (int)strlen(q) * GFX_CHAR_W) / 2;
        gfx_draw_text(qx, QUALITY_Y, q, gfx_rgb(100, 100, 100), 1);
    }

    /* transport controls */
    playlist_repeat_t repeat  = playlist_get_repeat();
    bool              shuffle = playlist_get_shuffle();
    gfx_color_t loop_col = (repeat  == PLAYLIST_REPEAT_OFF) ? gfx_rgb(60, 60, 60) : GFX_WHITE;
    gfx_color_t shuf_col = shuffle ? GFX_WHITE : gfx_rgb(60, 60, 60);
    const uint8_t *play_icon = (status.state == PLAYER_PLAYING) ? icon_pause : icon_play;

    gfx_blit_1bpp(CTRL_PREV_X, CTRL_Y, ICON_PREV_W, ICON_PREV_H, icon_prev,  GFX_WHITE);
    gfx_blit_1bpp(CTRL_PLAY_X, CTRL_Y, ICON_PLAY_W, ICON_PLAY_H, play_icon,  GFX_WHITE);
    gfx_blit_1bpp(CTRL_NEXT_X, CTRL_Y, ICON_NEXT_W, ICON_NEXT_H, icon_next,  GFX_WHITE);

    /* progress bar row */
    gfx_blit_1bpp(PROG_LOOP_X, PROG_ROW_Y, ICON_LOOP_W,    ICON_LOOP_H,    icon_loop,    loop_col);
    if (repeat == PLAYLIST_REPEAT_ONE)
        gfx_fill_rect(PROG_LOOP_X, PROG_ROW_Y + ICON_LOOP_H + 1, ICON_LOOP_W, 1, GFX_WHITE);
    if (status.total_ms > 0) {
        int fill_w = (int)((uint64_t)PROG_BAR_W * status.elapsed_ms / status.total_ms);
        if (fill_w > PROG_BAR_W) fill_w = PROG_BAR_W;
        if (fill_w > 0) gfx_fill_rect(PROG_BAR_X, PROG_BAR_Y, fill_w, PROG_BAR_H, GFX_WHITE);
    }
    gfx_draw_rect(PROG_BAR_X, PROG_BAR_Y, PROG_BAR_W, PROG_BAR_H, GFX_WHITE);
    gfx_blit_1bpp(SHUF_ICON_X, PROG_ROW_Y, ICON_SHUFFLE_W, ICON_SHUFFLE_H, icon_shuffle, shuf_col);

    /* cursor corners around the selected control (1px padding → 18×18) */
    draw_corners(k_ctrl[s_cursor].x - 1, k_ctrl[s_cursor].y - 1, 18, 18, GFX_WHITE);

    /* time display */
    char t_el[8], t_str[18];
    format_time(t_el, sizeof t_el, status.elapsed_ms);
    if (status.total_ms == 0) {
        snprintf(t_str, sizeof t_str, "%s / ?:??", t_el);
    } else {
        char t_tot[8];
        format_time(t_tot, sizeof t_tot, status.total_ms);
        snprintf(t_str, sizeof t_str, "%s / %s", t_el, t_tot);
    }
    int time_x = (CONT_W - (int)strlen(t_str) * GFX_CHAR_W) / 2;
    gfx_draw_text(time_x, TIME_Y, t_str, GFX_WHITE, 1);

    /* volume column */
    gfx_blit_1bpp(VOL_ICON_X, VOL_ICON_Y, ICON_VOLUME_W, ICON_VOLUME_H, icon_volume, GFX_WHITE);
    int vol_mv = 0;
    volume_poll(&vol_mv, NULL);
    int pct    = (int)adc_pot_to_avrcp_volume(vol_mv) * 100 / 127;
    int fill_h = VOL_BAR_H * pct / 100;
    if (fill_h > VOL_BAR_H) fill_h = VOL_BAR_H;
    if (fill_h > 0)
        gfx_fill_rect(VOL_BAR_X, VOL_BAR_Y + (VOL_BAR_H - fill_h), VOL_BAR_W, fill_h, GFX_WHITE);
    gfx_draw_rect(VOL_BAR_X,  VOL_BAR_Y,  VOL_BAR_W,     VOL_BAR_H,     GFX_WHITE);
}

screen_t *now_playing_screen(void)
{
    static screen_t s = {
        .on_enter     = np_enter,
        .on_exit      = np_exit,
        .handle_input = handle_input,
        .render       = render,
        .refresh_ms   = 250,
    };
    return &s;
}
