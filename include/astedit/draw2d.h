#ifndef ASTEDIT_DRAW2D_H_INCLUDED
#define ASTEDIT_DRAW2D_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/gfx.h>
#include <astedit/filepositions.h>

void flush_color_vertices(void);
void flush_subpixelRenderedFont_texture_vertices(void);
void push_color_vertices(struct ColorVertex2d *verts, int length);
void push_subpixelRenderedFont_texture_vertices(struct TextureVertex2d *verts, int length);
void push_rgba_texture_vertices(struct TextureVertex2d *verts, int length);
void begin_frame(int x, int y, int w, int h);
void end_frame(void);


struct DrawCursor {
        int xLeft;
        int fontSize;
        int distanceYtoBaseline;
        int lineHeight;
        /* We have switched to monospace.
         * That considerably simplifies calculations.
         * Currently there's no support for non-monospace. */
        int cellWidth;
        /**/
        int x;
        int lineY;
        FILEPOS codepointpos;
        FILEPOS lineNumber;  // not sure if that's a good idea
        int r;
        int g;
        int b;
        int a;
};

struct GuiRect {
        int x;
        int y;
        int w;
        int h;
};







void draw_colored_rect(int x, int y, int w, int h,
        unsigned r, unsigned g, unsigned b, unsigned a);

void draw_rgba_texture_rect(Texture texture,
        int x, int y, int w, int h,
        int texX, int texY, int texW, int texH);
void draw_subpixelRenderedFont_texture_rect(Texture texture,
        int r, int g, int b, int a,
        int x, int y, int w, int h,
        int texX, int texY, int texW, int texH);

#include <astedit/textedit.h>
void testdraw(struct TextEdit *textedit);

#endif
