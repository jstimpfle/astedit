#ifndef ASTEDIT_FONT_H_INCLUDED
#define ASTEDIT_FONT_H_INCLUDED

enum {
        ALIGN_LEFT,
        ALIGN_RIGHT,
        ALIGN_JUSTIFY,
        NUM_ALIGN_KINDS,
};

enum {
        FONTFACE_REGULAR,
        FONTFACE_BOLD,
        NUM_FONTFACES,
};




#include <stdint.h>

typedef int Font;
struct GlyphMeta {
        Font font;
        int size;
        uint32_t codepoint;
};

struct GlyphLayoutInfo {
        int pixW;
        int pixH;
        /* Almost a copy of the freetype values. Maybe not a good idea
        Difference: these values are in glyph raster pixel coordinates,
        i.e. already divided by 64. */
        int horiBearingX;
        int horiBearingY;
        int horiAdvance;  // XXX not accounted for kerning
};


struct DrawCursor {
        int xLeft;
        int fontSize;
        int ascender;
        int lineHeight;
        int x;
        int y;
        int codepointpos;
};

struct BoundingBox {
        float bbX;
        float bbY;
        float bbW;
        float bbH;
};


/*
layouts text span returns total length.
`text` is expected to point to an array of `length` codepoints.
If outPositions is not NULL, it is expected to be a positions array of length
length`. Each element of the array gets set to the corresponding character's
width.
*/
int measure_glyph_span(Font font, int size, const uint32_t *text, int length, int initX, int *outPositions);

int draw_glyphs_on_baseline(Font font, const struct BoundingBox *boundingBox,
        int size, const uint32_t *text, int length, int initX, int baselineY);



/* font-freetype.c */
void teardown_fonts(void);
void setup_fonts(void);
void render_glyph(const struct GlyphMeta *meta, unsigned char **outBuffer, int *outStride, struct GlyphLayoutInfo *outLayout);

#endif