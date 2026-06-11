#include "gfx.h"
#include "display_oled.h"

void gfx_clear(void)                              { display_oled_clear(); }
void gfx_draw_text(int x, int y, const char *text) { (void)x; (void)y; (void)text; }
void gfx_draw_rect(int x, int y, int w, int h)    { (void)x; (void)y; (void)w; (void)h; }
void gfx_flush(void)                              {}
