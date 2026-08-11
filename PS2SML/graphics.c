#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <gsKit.h>
#include <gsToolkit.h>
#include <dmaKit.h>

#include "graphics.h"
#include "config.h"
#include "font.h"

GSGLOBAL *gsGlobal = NULL;

static GSTEXTURE g_background;
static int g_background_loaded = 0;

static int ends_with_ci(const char *s, const char *suffix)
{
    size_t len_s;
    size_t len_suffix;

    if (!s || !suffix)
        return 0;

    len_s = strlen(s);
    len_suffix = strlen(suffix);

    if (len_suffix > len_s)
        return 0;

    s += len_s - len_suffix;

    while (*s && *suffix) {
        if (tolower((unsigned char)*s) !=
            tolower((unsigned char)*suffix)) {
            return 0;
        }

        ++s;
        ++suffix;
    }

    return 1;
}

static int load_background(const char *path)
{
    if (!path || !path[0])
        return 0;

    memset(&g_background, 0, sizeof(g_background));

    if (ends_with_ci(path, ".jpg") ||
        ends_with_ci(path, ".jpeg")) {

        if (gsKit_texture_jpeg_scale(gsGlobal,
                                     &g_background,
                                     (char *)path,
                                     true) < 0) {
            return 0;
        }

    } else if (ends_with_ci(path, ".png")) {

        if (gsKit_texture_png(gsGlobal,
                              &g_background,
                              (char *)path) < 0) {
            return 0;
        }

    } else {
        return 0;
    }

    g_background_loaded = 1;

    return 1;
}

int init_graphics(const char *video_mode)
{
    gsGlobal = gsKit_init_global();

    if (!gsGlobal)
        return 0;

    /*
     * Current gsKit does not provide gsKit_set_mode().
     * Set the video mode directly through GSGLOBAL.
     */
    if (video_mode &&
        strcasecmp(video_mode, "PAL") == 0) {

        gsGlobal->Mode = GS_MODE_PAL;

    } else if (video_mode &&
               strcasecmp(video_mode, "NTSC") == 0) {

        gsGlobal->Mode = GS_MODE_NTSC;

    } else {

        gsGlobal->Mode = gsKit_detect_signal();
    }

    gsGlobal->PSM = GS_PSM_CT32;
    gsGlobal->PSMZ = GS_PSMZ_16S;
    gsGlobal->DoubleBuffering = GS_SETTING_ON;
    gsGlobal->ZBuffering = GS_SETTING_OFF;
    gsGlobal->PrimAlphaEnable = GS_SETTING_ON;

    dmaKit_init(
        D_CTRL_RELE_OFF,
        D_CTRL_MFD_OFF,
        D_CTRL_STS_UNSPEC,
        D_CTRL_STD_OFF,
        D_CTRL_RCYC_8,
        1 << DMA_CHANNEL_GIF
    );

    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gsGlobal);

    gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    gsKit_set_test(gsGlobal, GS_ATEST_OFF);

    gsKit_clear(
        gsGlobal,
        GS_SETREG_RGBAQ(0, 0, 0, 0, 0)
    );

    gsKit_sync_flip(gsGlobal);

    /*
     * Background is optional.
     * A missing image must not prevent
     * the loader from starting.
     */
    if (config.bg_path &&
        config.bg_path[0] != '\0') {

        if (!load_background(config.bg_path)) {
            printf(
                "WARNING: background image could not be loaded: %s\n",
                config.bg_path
            );
        }
    }

    return 1;
}

static void draw_background(void)
{
    if (!g_background_loaded) {
        gsKit_clear(
            gsGlobal,
            GS_SETREG_RGBAQ(0, 0, 0, 0, 0)
        );

        return;
    }

    gsKit_set_primalpha(
        gsGlobal,
        GS_BLEND_BACK2FRONT,
        0
    );

    gsKit_set_test(
        gsGlobal,
        GS_ATEST_OFF
    );

    gsKit_prim_sprite_texture(
        gsGlobal,
        &g_background,

        0.0f,
        0.0f,

        0.0f,
        0.0f,

        (float)gsGlobal->Width,
        (float)gsGlobal->Height,

        (float)g_background.Width,
        (float)g_background.Height,

        0,

        GS_SETREG_RGBAQ(
            0x80,
            0x80,
            0x80,
            0x80,
            0
        )
    );
}

static int text_width(const char *text, int font_size)
{
    int width = 0;

    if (!text)
        return 0;

    while (*text) {

        /*
         * Do not count UTF-8 continuation bytes
         * as separate characters.
         */
        if ((*text & 0xC0) != 0x80)
            width += (font_size * 3) / 5;

        ++text;
    }

    return width;
}

void draw_menu(int selected, int show_description)
{
    int sw;
    int sh;

    int font_size;
    int line_height;
    int total_height;
    int start_y;

    int i;

    if (!gsGlobal)
        return;

    sw = gsGlobal->Width;
    sh = gsGlobal->Height;

    gsKit_clear(
        gsGlobal,
        GS_SETREG_RGBAQ(0, 0, 0, 0, 0)
    );

    draw_background();

    font_size = config.font_size;

    if (font_size < 8)
        font_size = 8;

    line_height = font_size + font_size / 2;

    total_height =
        config.item_count * line_height;

    while (total_height > sh - 100 &&
           font_size > 8) {

        --font_size;

        line_height =
            font_size + font_size / 2;

        total_height =
            config.item_count * line_height;
    }

    start_y =
        (sh - total_height) / 2;

    for (i = 0;
         i < config.item_count;
         ++i) {

        int y;
        int width;
        int x;

        y =
            start_y +
            i * line_height;

        width =
            text_width(
                config.items[i].title,
                font_size
            );

        x =
            (sw - width) / 2;

        if (x < 8)
            x = 8;

        draw_text(
            gsGlobal,
            config.items[i].title,
            x,
            y,
            font_size,
            (i == selected)
                ? config.color_active
                : config.color_inactive
        );
    }

    if (show_description &&
        config.item_count > 0) {

        const char *desc;

        int desc_size;
        int desc_width;
        int desc_x;

        desc =
            config.items[selected].desc;

        if (!desc)
            desc = "";

        desc_size =
            font_size - 4;

        if (desc_size < 8)
            desc_size = 8;

        gsKit_set_primalpha(
            gsGlobal,
            GS_BLEND_BACK2FRONT,
            0
        );

        gsKit_set_test(
            gsGlobal,
            GS_ATEST_OFF
        );

        gsKit_prim_sprite(
            gsGlobal,

            40.0f,
            (float)(sh - desc_size - 30),

            (float)(sw - 80),
            (float)(desc_size + 20),

            1,

            GS_SETREG_RGBAQ(
                0,
                0,
                0,
                0x80,
                0
            )
        );

        desc_width =
            text_width(
                desc,
                desc_size
            );

        desc_x =
            (sw - desc_width) / 2;

        if (desc_x < 48)
            desc_x = 48;

        draw_text(
            gsGlobal,
            desc,
            desc_x,
            sh - desc_size - 18,
            desc_size,
            0xFFFFFFFF
        );
    }

    gsKit_sync_flip(gsGlobal);
}

void cleanup_graphics(void)
{
    if (g_background.Mem) {
        free(g_background.Mem);
        g_background.Mem = NULL;
    }

    memset(
        &g_background,
        0,
        sizeof(g_background)
    );

    g_background_loaded = 0;

    if (gsGlobal) {
        gsKit_deinit_global(gsGlobal);
        gsGlobal = NULL;
    }
}
