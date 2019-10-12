#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/font.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define NUM_FONTFACES 2
static FT_Library  library;
static FT_Face faceKindTo_FT_Face[NUM_FONTFACES];



void render_glyph(const struct GlyphMeta *meta, unsigned char **outBuffer, int *outStride, struct GlyphLayoutInfo *outLayout)
{
        int error;
        uint32_t codepoint = meta->codepoint;
        FT_Face face = faceKindTo_FT_Face[meta->font];
        int size = meta->size;

        error = FT_Set_Char_Size(face,
                size * 64,   /* char_width in 1/64th of points */
                0,  /* char_height in 1/64th of points */
                96/*windowDPI_X*/,  /* horizontal device resolution */
                96/*windowDPI_Y*/); /* vertical device resolution */
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

#define USE_SUBPIXEL_RENDERING 1
#if USE_SUBPIXEL_RENDERING
        /* FT_RENDER_MODE_NORMAL means antialiased */
        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD);
#else
        /* FT_RENDER_MODE_NORMAL means antialiased */
        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
#endif
        if (error)
                /* TODO: does this mean OOM? */
                fatalf("Failed to render glyph for char %d\n", codepoint);

        FT_Bitmap *bitmap = &face->glyph->bitmap;

#if USE_SUBPIXEL_RENDERING
        if (bitmap->pixel_mode != FT_PIXEL_MODE_LCD)
                fatalf("Expected FT_PIXEL_MODE_LCD pixmap for subpixel rendering");
#else
	/* for simplicity, we assume that `bitmap->pixel_mode' */
	/* is `FT_PIXEL_MODE_GRAY' (i.e., not a bitmap font)   */
	if (bitmap->pixel_mode != FT_PIXEL_MODE_GRAY)
                fatalf("Assertion failed\n");
#endif

        log_postf("bitmap pixels (%c): %d %d", codepoint, bitmap->width, bitmap->rows);
        *outBuffer = bitmap->buffer;
        *outStride = bitmap->pitch;
        outLayout->pixW = bitmap->width;
        outLayout->pixH = bitmap->rows;
        outLayout->horiBearingX = (int) (face->glyph->metrics.horiBearingX / 64.0f);
        outLayout->horiBearingY = (int) (face->glyph->metrics.horiBearingY / 64.0f);
        outLayout->horiAdvance = (int) (face->glyph->metrics.horiAdvance / 64.0f);
}



static const char *faceKindToFontpath[NUM_FONTFACES] = {
        "fontfiles/NotoSans/NotoSans-Regular.ttf",
        "fontfiles/NotoSans/NotoSans-Bold.ttf",
};


void setup_fonts(void)
{
        {
                int error = FT_Init_FreeType(&library);
                if (error)
                        fatal("Failed to initialize the FreeType font library\n");
        }

        for (int i = 0; i < NUM_FONTFACES; i++) {
                int error = FT_New_Face(library, faceKindToFontpath[i], 0,
                                                &faceKindTo_FT_Face[i]);
                if (error == FT_Err_Unknown_File_Format)
                        fatal("Unsupported font file format\n");
                else if (error)
                        fatal("Error reading font file\n");
        }
}

void teardown_fonts(void)
{
        for (int i = 0; i < NUM_FONTFACES; i++)
                FT_Done_Face(faceKindTo_FT_Face[i]);
        FT_Done_FreeType(library);
}
