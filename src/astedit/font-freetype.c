#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/logging.h>
#include <astedit/memoryalloc.h>
#include <astedit/font.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftlcdfil.h>
#include <freetype/ftdriver.h>
#include <freetype/ftmodapi.h>

#define USE_SUBPIXEL_RENDERING 1

static const char *faceKindToFontpath[NUM_FONTFACES] = {
        //[FONTFACE_REGULAR] = "NotoSans/NotoSans-Regular.ttf",
        [FONTFACE_REGULAR] ="NotoMono/NotoMono-Regular.ttf",
        [FONTFACE_BOLD] ="NotoSans/NotoSans-Bold.ttf",
};

static FT_Library  library;
static FT_Face faceKindTo_FT_Face[NUM_FONTFACES];

void render_glyph(const struct GlyphMeta *meta, unsigned char **outBuffer, int *outStride, struct GlyphLayoutInfo *outLayout)
{
        int error;
        uint32_t codepoint = meta->codepoint;
        FT_Face face = faceKindTo_FT_Face[meta->font];
        int size = meta->size;

        // interpret the size as max height of character in screen pixels
        error = FT_Set_Char_Size(face,
                0,   /* char_width in 1/64th of points */
                size * 64,  /* char_height in 1/64th of points */
                72,  /* horizontal device resolution */
                72); /* vertical device resolution */
        if (error)
                fatal("Failed to set FreeType char size\n");

        int glyph_index = FT_Get_Char_Index(face, codepoint);
        error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        if (error) {
                glyph_index = FT_Get_Char_Index(face, 0xFFFD);
                error = FT_Load_Glyph(face, glyph_index, FT_LOAD_COLOR);
                if (error)
                        fatalf("Failed to load glyph for char %d\n", codepoint);
        }

        if (USE_SUBPIXEL_RENDERING)
                error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD); /* subpixel rendering for R-G-B pixels */
        else
                error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL); /* antialiased */
        if (error) /* TODO: does this mean OOM? */
                fatalf("Failed to render glyph for char %d\n", codepoint);

        FT_Bitmap *bitmap = &face->glyph->bitmap;
        if (USE_SUBPIXEL_RENDERING) {
                if (bitmap->pixel_mode != FT_PIXEL_MODE_LCD)  /* rgb bitmap for subpixel rendering */
                        fatalf("Expected FT_PIXEL_MODE_LCD pixmap for subpixel rendering");
        }
        else {
                /* FT_PIXEL_MODE_GRAY = 8-bit (antialiased) graymap */
                if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY)
                        fatalf("Assertion failed\n");
        }

        *outBuffer = bitmap->buffer;
        *outStride = bitmap->pitch;
        outLayout->pixW = bitmap->width;
        outLayout->pixH = bitmap->rows;
        outLayout->horiBearingX = (int) (face->glyph->metrics.horiBearingX / 64.0f);
        outLayout->horiBearingY = (int) (face->glyph->metrics.horiBearingY / 64.0f);
        outLayout->horiAdvance = (int) (face->glyph->metrics.horiAdvance / 64.0f);
}

static char *fontpath;
static int fontpathLength;

static void make_fontpath_from_pathspec(const char *pathspec)
{
        int fontdirLength = (int) strlen(configuredFontdir);
        int specLength = (int) strlen(pathspec);
        fontpathLength = fontdirLength + 1 + specLength + 1;
        REALLOC_MEMORY(&fontpath, fontpathLength);
        copy_memory(fontpath, configuredFontdir, fontdirLength);
        copy_memory(fontpath + fontdirLength, "/", 1);
        copy_memory(fontpath + fontdirLength + 1, pathspec, specLength);
        copy_memory(fontpath + fontdirLength + 1 + specLength, "", 1); // zero-terminate
}

static void release_fontpath(void)
{
        FREE_MEMORY(&fontpath);
        fontpathLength = 0;
}

void setup_fonts(void)
{
        {
                int error = FT_Init_FreeType(&library);
                if (error)
                        fatal("Failed to initialize the FreeType font library\n");
        }

#if 0
        {
                FT_UInt interp_ver = TT_INTERPRETER_VERSION_40;
                FT_Error err = FT_Property_Set(library, "truetype", "interpreter-version", &interp_ver);
                if (err)
                        log_postf("Warning: Freetype error %d returned from "
                                  "FT_Property_Set() when setting "
                                  "TT_INTERPRETER_VERSION_40. Visual quality might be drastically degraded.", err);
        }
#endif

        FT_Library_SetLcdFilter(library, FT_LCD_FILTER_DEFAULT);  // Needed for subpixel rendering to work

        for (int i = 0; i < NUM_FONTFACES; i++) {
                make_fontpath_from_pathspec(faceKindToFontpath[i]);
                int error = FT_New_Face(library, fontpath, 0, &faceKindTo_FT_Face[i]);
                if (error == FT_Err_Unknown_File_Format)
                        fatal("Unsupported font file format\n");
                else if (error)
                        fatal("Error reading font file\n");
        }
        release_fontpath();
}

void teardown_fonts(void)
{
        for (int i = 0; i < NUM_FONTFACES; i++)
                FT_Done_Face(faceKindTo_FT_Face[i]);
        FT_Done_FreeType(library);
}
