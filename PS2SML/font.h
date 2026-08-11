#ifndef FONT_H
#define FONT_H

#include <gsKit.h>

int init_font(const char *font_path);
void draw_text(GSGLOBAL *gsGlobal,
               const char *text,
               int x,
               int y,
               int size,
               unsigned int color);
void cleanup_font(void);

#endif
