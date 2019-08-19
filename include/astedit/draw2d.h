#ifndef ASTEDIT_DRAW2D_H_INCLUDED
#define ASTEDIT_DRAW2D_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/gfx.h>
#include <astedit/filepositions.h>

void flush_color_vertices(void);
void flush_alpha_texture_vertices(void);
void push_color_vertices(struct ColorVertex2d *verts, int length);
void push_alpha_texture_vertices(struct TextureVertex2d *verts, int length);
void push_rgba_texture_vertices(struct TextureVertex2d *verts, int length);
void begin_frame(int x, int y, int w, int h);
void end_frame(void);


struct DrawCursor {
        int xLeft;
        int fontSize;
        int ascender;
        int lineHeight;
        int x;
        int y;
        FILEPOS codepointpos;
        FILEPOS lineNumber;  // not sure if that's a good idea
};

struct BoundingBox {
        int bbX;
        int bbY;
        int bbW;
        int bbH;
};







void draw_colored_rect(int x, int y, int w, int h,
        unsigned r, unsigned g, unsigned b, unsigned a);

void draw_rgba_texture_rect(Texture texture,
        int x, int y, int w, int h,
        float texX, float texY, float texW, float texH);
void draw_alpha_texture_rect(Texture texture,
        int r, int g, int b, int a,
        int x, int y, int w, int h,
        float texX, float texY, float texW, float texH);

#include <astedit/textedit.h>
void testdraw(struct TextEdit *textedit);

#endif
