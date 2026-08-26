#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"

Config config;

static void copy_string(char *dst, size_t dst_size, const char *src)
{
    if (dst_size == 0)
        return;

    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static char *trim(char *s)
{
    char *end;

    while (*s && isspace((unsigned char)*s))
        ++s;

    if (*s == '\0')
        return s;

    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        --end;
    }

    return s;
}

static unsigned int hex_to_rgba(const char *hex, unsigned int fallback)
{
    unsigned int value;
    char *end;

    if (*hex == '#')
        ++hex;

    value = (unsigned int)strtoul(hex, &end, 16);

    if (end == hex || *end != '\0')
        return fallback;

    /*
     * Configuration format:
     *   RRGGBB
     *
     * gsKit uses RGBA byte order in the low 32 bits.
     */
    if ((size_t)(end - hex) <= 6)
        value = (value << 8) | 0xFF;

    return value;
}

static int has_device_prefix(const char *path)
{
    return strchr(path, ':') != NULL;
}

static void make_config_path(char *dst, size_t dst_size, const char *value)
{
    value = trim((char *)value);

    if (has_device_prefix(value)) {
        copy_string(dst, dst_size, value);
    } else {
        snprintf(dst, dst_size, "cdrom0:\\PS2SML\\%s", value);
    }
}

static void make_item_path(char *dst, size_t dst_size, const char *value)
{
    value = trim((char *)value);

    if (has_device_prefix(value)) {
        copy_string(dst, dst_size, value);
    } else {
        snprintf(dst, dst_size, "cdrom0:\\%s", value);
    }
}

static void set_defaults(void)
{
    memset(&config, 0, sizeof(config));

    copy_string(config.video_mode, sizeof(config.video_mode), "AUTO");
    config.font_size = 24;

    config.color_inactive = 0xFFFFFFFF;
    config.color_active   = 0xFF0000FF;

    copy_string(config.bg_path, sizeof(config.bg_path),
                "cdrom0:\\PS2SML\\bg.png");

    copy_string(config.font_path, sizeof(config.font_path),
                "cdrom0:\\PS2SML\\font.ttf");
}

int load_config(void)
{
    FILE *f;
    char line[MAX_LINE];
    int in_items = 0;

    set_defaults();

    f = fopen("cdrom0:\\PS2SML\\conf.cfg", "r");
    if (!f)
        return 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        char *p = trim(line);

        if (*p == '\0' || *p == ';' || *p == '#')
            continue;

        if (*p == '[') {
            if (strcmp(p, "[Settings]") == 0)
                in_items = 0;
            else if (strcmp(p, "[Items]") == 0)
                in_items = 1;
            continue;
        }

        if (!in_items) {
            char *eq = strchr(p, '=');
            char *key;
            char *value;

            if (!eq)
                continue;

            *eq = '\0';
            key = trim(p);
            value = trim(eq + 1);

            if (strcmp(key, "VideoMode") == 0) {
                if (strcasecmp(value, "PAL") == 0)
                    copy_string(config.video_mode, sizeof(config.video_mode), "PAL");
                else if (strcasecmp(value, "NTSC") == 0)
                    copy_string(config.video_mode, sizeof(config.video_mode), "NTSC");
                else
                    copy_string(config.video_mode, sizeof(config.video_mode), "AUTO");
            }
            else if (strcmp(key, "FontSize") == 0) {
                int size = atoi(value);
                if (size >= 8 && size <= 64)
                    config.font_size = size;
            }
            else if (strcmp(key, "ColorInactive") == 0) {
                config.color_inactive =
                    hex_to_rgba(value, config.color_inactive);
            }
            else if (strcmp(key, "ColorActive") == 0) {
                config.color_active =
                    hex_to_rgba(value, config.color_active);
            }
            else if (strcmp(key, "BackgroundImage") == 0) {
                make_config_path(config.bg_path, sizeof(config.bg_path), value);
            }
            else if (strcmp(key, "FontFile") == 0) {
                make_config_path(config.font_path, sizeof(config.font_path), value);
            }

            continue;
        }

        if (config.item_count < MAX_ITEMS) {
            char *title = strtok(p, "|");
            char *desc  = strtok(NULL, "|");
            char *path  = strtok(NULL, "|");

            if (title && desc && path) {
                MenuItem *item = &config.items[config.item_count];

                title = trim(title);
                desc  = trim(desc);
                path  = trim(path);

                copy_string(item->title, sizeof(item->title), title);
                copy_string(item->desc, sizeof(item->desc), desc);
                make_item_path(item->path, sizeof(item->path), path);

                ++config.item_count;
            }
        }
    }

    fclose(f);

    return config.item_count > 0;
}
