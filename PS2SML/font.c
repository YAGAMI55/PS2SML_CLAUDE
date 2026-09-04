#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "font.h"

#define MAX_GLYPHS 256

typedef struct
{
    int valid;

    unsigned int codepoint;
    int size;

    GSTEXTURE texture;

    int width;
    int height;

    int bitmap_left;
    int bitmap_top;

    int advance_x;
} GlyphCacheEntry;

static FT_Library g_library = NULL;
static FT_Face g_face = NULL;
static unsigned char *g_font_buffer = NULL;

static GlyphCacheEntry g_cache[MAX_GLYPHS];


/*
 * Decode one UTF-8 codepoint.
 *
 * The loader configuration and menu strings are expected to be UTF-8.
 *
 * Invalid UTF-8 sequences are replaced with '?'.
 */
static unsigned int utf8_next(const char **text)
{
    const unsigned char *p;
    unsigned int cp;

    if (!text || !*text)
        return 0;

    p = (const unsigned char *)*text;

    if (*p == '\0')
        return 0;

    /*
     * ASCII
     */
    if (p[0] < 0x80)
    {
        ++*text;
        return p[0];
    }

    /*
     * 2-byte UTF-8 sequence
     */
    if ((p[0] & 0xE0) == 0xC0)
    {
        if (p[1] >= 0x80 && p[1] <= 0xBF)
        {
            cp =
                ((unsigned int)(p[0] & 0x1F) << 6) |
                (unsigned int)(p[1] & 0x3F);

            /*
             * Reject overlong encodings.
             */
            if (cp >= 0x80)
            {
                *text += 2;
                return cp;
            }
        }

        ++*text;
        return '?';
    }

    /*
     * 3-byte UTF-8 sequence
     */
    if ((p[0] & 0xF0) == 0xE0)
    {
        if (p[1] >= 0x80 && p[1] <= 0xBF &&
            p[2] >= 0x80 && p[2] <= 0xBF)
        {
            cp =
                ((unsigned int)(p[0] & 0x0F) << 12) |
                ((unsigned int)(p[1] & 0x3F) << 6) |
                (unsigned int)(p[2] & 0x3F);

            /*
             * Reject overlong sequences and UTF-16 surrogate range.
             */
            if (cp >= 0x800 &&
                !(cp >= 0xD800 && cp <= 0xDFFF))
            {
                *text += 3;
                return cp;
            }
        }

        ++*text;
        return '?';
    }

    /*
     * 4-byte UTF-8 sequence.
     */
    if ((p[0] & 0xF8) == 0xF0)
    {
        if (p[1] >= 0x80 && p[1] <= 0xBF &&
            p[2] >= 0x80 && p[2] <= 0xBF &&
            p[3] >= 0x80 && p[3] <= 0xBF)
        {
            cp =
                ((unsigned int)(p[0] & 0x07) << 18) |
                ((unsigned int)(p[1] & 0x3F) << 12) |
                ((unsigned int)(p[2] & 0x3F) << 6) |
                (unsigned int)(p[3] & 0x3F);

            /*
             * Unicode maximum is U+10FFFF.
             */
            if (cp >= 0x10000 && cp <= 0x10FFFF)
            {
                *text += 4;
                return cp;
            }
        }

        ++*text;
        return '?';
    }

    ++*text;
    return '?';
}


/*
 * Find an already rendered glyph in the cache.
 */
static GlyphCacheEntry *find_glyph(unsigned int codepoint, int size)
{
    int i;

    for (i = 0; i < MAX_GLYPHS; ++i)
    {
        if (g_cache[i].valid &&
            g_cache[i].codepoint == codepoint &&
            g_cache[i].size == size)
        {
            return &g_cache[i];
        }
    }

    return NULL;
}


/*
 * Find an unused cache slot.
 */
static GlyphCacheEntry *allocate_slot(void)
{
    int i;

    for (i = 0; i < MAX_GLYPHS; ++i)
    {
        if (!g_cache[i].valid)
            return &g_cache[i];
    }

    return NULL;
}


/*
 * Release the EE-side memory associated with a cache entry.
 *
 * VRAM itself is owned by gsKit's VRAM allocator and will be released
 * when the graphics subsystem is destroyed.
 */
static void release_cache_entry(GlyphCacheEntry *entry)
{
    if (!entry)
        return;

    if (entry->texture.Mem)
    {
        free(entry->texture.Mem);
        entry->texture.Mem = NULL;
    }

    memset(entry, 0, sizeof(*entry));
}


/*
 * Render one glyph using FreeType and upload it to GS VRAM.
 */
static GlyphCacheEntry *build_glyph(GSGLOBAL *gsGlobal,
                                    unsigned int codepoint,
                                    int size)
{
    GlyphCacheEntry *entry;
    size_t tex_size;
    unsigned int x;
    unsigned int y;
    int pitch;

    if (!gsGlobal || !g_face || size <= 0)
        return NULL;

    /*
     * Check cache first.
     */
    entry = find_glyph(codepoint, size);

    if (entry)
        return entry;

    /*
     * Allocate a new cache entry.
     */
    entry = allocate_slot();

    if (!entry)
    {
        /*
         * Cache is full.
         *
         * For now we do not evict glyphs automatically because the
         * textures are already resident in GS VRAM. The cache will be
         * cleared when the font is destroyed.
         */
        return NULL;
    }

    memset(entry, 0, sizeof(*entry));

    /*
     * Set requested pixel size.
     */
    if (FT_Set_Pixel_Sizes(g_face,
                           0,
                           (FT_UInt)size) != 0)
    {
        memset(entry, 0, sizeof(*entry));
        return NULL;
    }

    /*
     * Load and rasterize glyph.
     *
     * FT_LOAD_RENDER causes FreeType to generate an 8-bit grayscale
     * bitmap in glyph->bitmap.
     */
    if (FT_Load_Char(g_face,
                     (FT_ULong)codepoint,
                     FT_LOAD_RENDER) != 0)
    {
        memset(entry, 0, sizeof(*entry));
        return NULL;
    }

    entry->codepoint = codepoint;
    entry->size = size;

    entry->width =
        (int)g_face->glyph->bitmap.width;

    entry->height =
        (int)g_face->glyph->bitmap.rows;

    entry->bitmap_left =
        (int)g_face->glyph->bitmap_left;

    entry->bitmap_top =
        (int)g_face->glyph->bitmap_top;

    entry->advance_x =
        (int)(g_face->glyph->advance.x >> 6);

    /*
     * Make sure even an empty glyph has a useful advance.
     *
     * This is particularly important for:
     *
     *     ' '
     *     '\t'
     *     some combining characters
     */
    if (entry->advance_x < 0)
        entry->advance_x = 0;

    /*
     * Space and other invisible glyphs can legitimately have no bitmap.
     */
    if (entry->width <= 0 ||
        entry->height <= 0)
    {
        entry->width = 0;
        entry->height = 0;
        entry->valid = 1;

        return entry;
    }

    /*
     * Prepare a GS 32-bit texture.
     */
    memset(&entry->texture,
           0,
           sizeof(entry->texture));

    entry->texture.Width =
        (u32)entry->width;

    entry->texture.Height =
        (u32)entry->height;

    entry->texture.PSM =
        GS_PSM_CT32;

    entry->texture.Filter =
        GS_FILTER_LINEAR;

    entry->texture.VramClut =
        0;

    entry->texture.Clut =
        NULL;

    /*
     * Allocate EE-side texture buffer.
     *
     * gsKit expects properly aligned texture memory.
     */
    tex_size =
        gsKit_texture_size_ee(entry->width,
                              entry->height,
                              GS_PSM_CT32);

    entry->texture.Mem =
        (u32 *)memalign(128, tex_size);

    if (!entry->texture.Mem)
    {
        memset(entry, 0, sizeof(*entry));
        return NULL;
    }

    memset(entry->texture.Mem,
           0,
           tex_size);

    /*
     * FreeType's grayscale bitmap uses one byte per pixel.
     *
     * Convert it to:
     *
     *     RGB = white
     *     A   = glyph coverage
     *
     * The actual requested text color is supplied later to
     * gsKit_prim_sprite_texture().
     */
    pitch =
        g_face->glyph->bitmap.pitch;

    if (pitch < 0)
        pitch = -pitch;

    for (y = 0;
         y < (unsigned int)entry->height;
         ++y)
    {
        for (x = 0;
             x < (unsigned int)entry->width;
             ++x)
        {
            unsigned char alpha;

            alpha =
                g_face->glyph->bitmap.buffer[
                    y * pitch + x
                ];

            /*
             * RGB = 255,255,255
             * A   = FreeType coverage
             */
            ((u32 *)entry->texture.Mem)
                [y * (unsigned int)entry->width + x] =
                    ((u32)alpha << 24) | 0x00FFFFFFu;
        }
    }

    /*
     * Allocate GS VRAM.
     */
    entry->texture.Vram =
        gsKit_vram_alloc(
            gsGlobal,
            gsKit_texture_size(
                entry->width,
                entry->height,
                GS_PSM_CT32),
            GSKIT_ALLOC_USERBUFFER);

    if (entry->texture.Vram ==
        GSKIT_ALLOC_ERROR)
    {
        release_cache_entry(entry);
        return NULL;
    }

    /*
     * Upload texture to GS.
     */
    gsKit_texture_upload(
        gsGlobal,
        &entry->texture);

    /*
     * The EE-side copy is no longer needed.
     *
     * GS now owns the uploaded texture data in VRAM.
     */
    free(entry->texture.Mem);
    entry->texture.Mem = NULL;

    entry->valid = 1;

    return entry;
}


/*
 * Initialize FreeType and load the requested font.
 *
 * The font may be a TrueType (.ttf) or OpenType (.otf) font,
 * provided that the FreeType build used by the PS2 toolchain
 * supports that font format.
 */
int init_font(const char *font_path)
{
    FT_Error error;

    /*
     * Reset state in case the function is called more than once.
     */
    cleanup_font();

    if (!font_path || !font_path[0])
        return 0;

    /*
     * Load the entire font file into RAM once.
     *
     * FT_New_Face() would stream glyph/table data directly from
     * cdrom0: on every access, which is extremely slow on real
     * PS2 CD/DVD hardware (and emulated drives). Loading the whole
     * file up front and using FT_New_Memory_Face() means FreeType
     * only ever touches RAM afterwards.
     */
    {
        FILE *f;
        long size;

        f = fopen(font_path, "rb");

        if (!f)
        {
            printf("ERROR: could not open font: %s\n", font_path);
            return 0;
        }

        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (size <= 0)
        {
            fclose(f);
            printf("ERROR: font file is empty: %s\n", font_path);
            return 0;
        }

        g_font_buffer = (unsigned char *)malloc((size_t)size);

        if (!g_font_buffer)
        {
            fclose(f);
            printf("ERROR: out of memory loading font: %s\n", font_path);
            return 0;
        }

        if (fread(g_font_buffer, 1, (size_t)size, f) != (size_t)size)
        {
            fclose(f);
            free(g_font_buffer);
            g_font_buffer = NULL;
            printf("ERROR: failed reading font: %s\n", font_path);
            return 0;
        }

        fclose(f);

        /*
         * Initialize FreeType.
         */
        error =
            FT_Init_FreeType(&g_library);

        if (error != 0)
        {
            g_library = NULL;

            free(g_font_buffer);
            g_font_buffer = NULL;

            printf("ERROR: FT_Init_FreeType failed: %d\n",
                   (int)error);

            return 0;
        }

        /*
         * Load first face from the in-memory font buffer.
         *
         * Both TTF and OTF are handled by FreeType.
         */
        error =
            FT_New_Memory_Face(g_library,
                               g_font_buffer,
                               (FT_Long)size,
                               0,
                               &g_face);

        if (error != 0)
        {
            printf("ERROR: could not load font: %s\n",
                   font_path);

            printf("ERROR: FreeType error: %d\n",
                   (int)error);

            FT_Done_FreeType(g_library);

            g_library = NULL;
            g_face = NULL;

            free(g_font_buffer);
            g_font_buffer = NULL;

            return 0;
        }
    }

    /*
     * Select Unicode charmap when available.
     *
     * Most modern TTF/OTF fonts expose a Unicode charmap.
     */
    if (FT_Select_Charmap(g_face,
                          FT_ENCODING_UNICODE) != 0)
    {
        /*
         * Do not fail immediately.
         *
         * Some fonts may still work with the currently selected
         * charmap.
         */
        printf("WARNING: font has no Unicode charmap: %s\n",
               font_path);
    }

    /*
     * Clear glyph cache.
     */
    memset(g_cache,
           0,
           sizeof(g_cache));

    printf("Font loaded: %s\n",
           font_path);

    return 1;
}


/*
 * Draw UTF-8 text.
 *
 * Coordinates:
 *
 *     x = left baseline position
 *     y = baseline
 *
 * 'size' is the requested pixel height.
 */
void draw_text(GSGLOBAL *gsGlobal,
               const char *text,
               int x,
               int y,
               int size,
               unsigned int color)
{
    const char *p;
    int pen_x;
    int baseline_y;
    u64 gs_color;

    if (!gsGlobal ||
        !g_face ||
        !text ||
        size <= 0)
    {
        return;
    }

    p = text;

    pen_x = x;
    baseline_y = y;

    gs_color =
        (u64)color;

    /*
     * Enable alpha testing for glyph textures.
     */
    gsKit_set_primalpha(
        gsGlobal,
        GS_BLEND_BACK2FRONT,
        0);

    gsKit_set_test(
        gsGlobal,
        GS_ATEST_ON);

    while (*p)
    {
        unsigned int codepoint;
        GlyphCacheEntry *glyph;

        codepoint =
            utf8_next(&p);

        if (codepoint == 0)
            break;

        /*
         * New line.
         */
        if (codepoint == '\n')
        {
            baseline_y +=
                size +
                size / 4;

            pen_x = x;

            continue;
        }

        /*
         * Ignore carriage return.
         */
        if (codepoint == '\r')
            continue;

        /*
         * Tab.
         *
         * Treat it as four spaces.
         */
        if (codepoint == '\t')
        {
            GlyphCacheEntry *space;

            space =
                build_glyph(
                    gsGlobal,
                    ' ',
                    size);

            if (space)
            {
                pen_x +=
                    space->advance_x * 4;
            }

            continue;
        }

        /*
         * Get rendered glyph from cache or build it.
         */
        glyph =
            build_glyph(
                gsGlobal,
                codepoint,
                size);

        if (!glyph)
        {
            /*
             * Missing glyph.
             *
             * Try '?' as a fallback.
             */
            if (codepoint != '?')
            {
                glyph =
                    build_glyph(
                        gsGlobal,
                        '?',
                        size);
            }
        }

        if (!glyph)
            continue;

        /*
         * Draw bitmap.
         */
        if (glyph->width > 0 &&
            glyph->height > 0)
        {
            int draw_x;
            int draw_y;

            draw_x =
                pen_x +
                glyph->bitmap_left;

            draw_y =
                baseline_y -
                glyph->bitmap_top;

            gsKit_prim_sprite_texture(
                gsGlobal,
                &glyph->texture,

                (float)draw_x,
                (float)draw_y,

                0.0f,
                0.0f,

                (float)(draw_x +
                        glyph->width),

                (float)(draw_y +
                        glyph->height),

                (float)glyph->width,
                (float)glyph->height,

                1,
                gs_color);
        }

        /*
         * Advance cursor according to the actual font metrics.
         */
        pen_x +=
            glyph->advance_x;
    }
}


/*
 * Release all font resources.
 */
void cleanup_font(void)
{
    int i;

    /*
     * Release EE-side buffers and invalidate cache.
     *
     * The GS VRAM allocations are owned by gsKit and are released
     * when the graphics subsystem is deinitialized.
     */
    for (i = 0;
         i < MAX_GLYPHS;
         ++i)
    {
        if (g_cache[i].texture.Mem)
        {
            free(g_cache[i].texture.Mem);
            g_cache[i].texture.Mem = NULL;
        }

        memset(&g_cache[i],
               0,
               sizeof(g_cache[i]));
    }

    /*
     * Release font face.
     */
    if (g_face)
    {
        FT_Done_Face(g_face);
        g_face = NULL;
    }

    /*
     * Release FreeType library.
     */
    if (g_library)
    {
        FT_Done_FreeType(g_library);
        g_library = NULL;
    }

    /*
     * Release the in-memory font buffer.
     *
     * Must be freed only after FT_Done_Face()/FT_Done_FreeType(),
     * since the face keeps pointers into this buffer while it is
     * in use.
     */
    if (g_font_buffer)
    {
        free(g_font_buffer);
        g_font_buffer = NULL;
    }
}
