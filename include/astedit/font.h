#ifndef ASTEDIT_FONT_H_INCLUDED
#define ASTEDIT_FONT_H_INCLUDED

#include <stdint.h>
#include <astedit/gfx.h>  // XXX needed for texture

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


DATA const char *configuredFontdir;

typedef int Font;

struct TexDrawInfo {
        Texture tex;
        int horiAdvance;  // TODO: try to find way to get this setting globally. We have monospace fonts!
        int bearingX;
        int bearingY;
        int texX;
        int texY;
        int texW;
        int texH;
};

void get_TexDrawInfo_for_glyph(Font font, int size, uint32_t codepoint, struct TexDrawInfo *out);


/*
layouts text span returns total length.
`cellWidth` is expected to be the cell width in case of a monospaced font,
or -1 otherwise.
`text` is expected to point to an array of `length` codepoints.
If outPositions is not NULL, it is expected to be a positions array of length
length`. Each element of the array gets set to the corresponding character's
width.
*/
/* XXX: should we remove this function now that we have more or less committed to monospaced fonts? */
int measure_glyph_span(Font font, int size, int cellWidth,
        const uint32_t *text, int length, int initX, int *outPositions);

void setup_font_atlas(void);
void teardown_font_atlas(void);


/* font-freetype.c */
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
void teardown_fonts(void);
void setup_fonts(void);
void render_glyph(const struct GlyphMeta *meta, unsigned char **outBuffer, int *outStride, struct GlyphLayoutInfo *outLayout);

#endif
