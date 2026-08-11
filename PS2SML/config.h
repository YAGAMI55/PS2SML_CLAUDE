#ifndef CONFIG_H
#define CONFIG_H

#define MAX_ITEMS 30
#define MAX_LINE 256

#define TITLE_LENGTH 64
#define DESC_LENGTH 256
#define PATH_LENGTH 256
#define VIDEO_MODE_LENGTH 8

typedef struct {
    char title[TITLE_LENGTH];
    char desc[DESC_LENGTH];
    char path[PATH_LENGTH];
} MenuItem;

typedef struct {
    char video_mode[VIDEO_MODE_LENGTH];
    int font_size;
    unsigned int color_inactive;
    unsigned int color_active;
    char bg_path[PATH_LENGTH];
    char font_path[PATH_LENGTH];
    MenuItem items[MAX_ITEMS];
    int item_count;
} Config;

extern Config config;

int load_config(void);

#endif
