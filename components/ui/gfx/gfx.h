#pragma once

#include <stdint.h>

void gfx_clear(void);
void gfx_draw_text(int x, int y, const char *text);
void gfx_draw_rect(int x, int y, int w, int h);
void gfx_flush(void);
