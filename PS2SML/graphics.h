#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <gsKit.h>

extern GSGLOBAL *gsGlobal;

int init_graphics(const char *video_mode);
void draw_menu(int selected, int show_description);
void cleanup_graphics(void);

#endif
