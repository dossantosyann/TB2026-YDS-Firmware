#pragma once
/* Minimal host stand-in for ESP-IDF's esp_attr.h: the section-placement attributes are a
   target concern (PSRAM/IRAM), so off-target they expand to nothing and the data lands in
   ordinary BSS. Only what the host-tested sources use is defined here. */

#define EXT_RAM_BSS_ATTR   /* gfx.c: framebuffer in PSRAM on target, plain BSS on host */
