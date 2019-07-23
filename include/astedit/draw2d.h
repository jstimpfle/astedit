#ifndef ASTEDIT_DRAW2D_H_INCLUDED
#define ASTEDIT_DRAW2D_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/gfx.h>

void flush_color_vertices(void);
void flush_alpha_texture_vertices(void);
void push_color_vertices(struct ColorVertex2d *verts, int length);
void push_alpha_texture_vertices(struct TextureVertex2d *verts, int length);
void push_rgba_texture_vertices(struct TextureVertex2d *verts, int length);
void begin_frame(int x, int y, int w, int h);
void end_frame(void);

void draw_colored_rect(int x, int y, int w, int h,
        unsigned r, unsigned g, unsigned b, unsigned a);

void draw_rgba_texture_rect(int x, int y, int w, int h,
        float texX, float texY, float texW, float texH, Texture texture);
void draw_alpha_texture_rect(int x, int y, int w, int h,
        float texX, float texY, float texW, float texH, Texture texture);

#include <astedit/textedit.h>
void testdraw(struct TextEdit *textedit);

#endif
