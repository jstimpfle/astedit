#ifndef ASTEDIT_GFX_H_INCLUDED
#define ASTEDIT_GFX_H_INCLUDED

/*
 * We can have a dynamic number of textures. They can be created, destroyed, and drawn, at will.
 * TODO: We probably don't want to expose the distinction between RGBA Textures and alpha textures here.
 */
typedef int Texture;



/* These newer, higher-level interfaces should supersede the ColorVertex2d
 * and TextureVertex2d structures soon. */

struct LayedOutGlyph {
        Texture tex;
        int tx;
        int ty;
        int tw;
        int th;
        int x;
        int y;
        // We should probably avoid storing the color here
        int r;
        int g;
        int b;
        int a;
};

struct LayedOutRect {
        int x;
        int y;
        int w;
        int h;
        // We should probably avoid storing the color here
        int r;
        int g;
        int b;
        int a;
};


void setup_gfx(void);
void teardown_gfx(void);

void clear_screen_and_drawing_state(void);
void flush_gfx(void);
void set_2d_coordinate_system(int x, int y, int w, int h);
void set_viewport_in_pixels(int x, int y, int w, int h);
void set_clipping_rect_in_pixels(int x, int y, int w, int h);
void clear_clipping_rect(void);
void gfx_toggle_srgb(void);  /* toggle automatic sRGB conversion on output to Framebuffer */

Texture create_rgba_texture(int w, int h);
Texture create_rgb_texture(int w, int h);
Texture create_alpha_texture(int w, int h);
void destroy_texture(Texture texHandle);

void upload_rgba_texture_data(Texture texture, const unsigned char *data, int size, int w, int h);
void upload_alpha_texture_data(Texture texture, const unsigned char *data, int size, int w, int h);
void update_alpha_texture_subimage(Texture texture, int row, int numRows, int rowWidth, int stride, const unsigned char *data);
void update_rgb_texture_subimage(Texture texture, int row, int numRows, int rowWidth, int stride, const unsigned char *data);

void draw_glyphs(struct LayedOutGlyph *glyphs, int numGlyphs);
void draw_rects(struct LayedOutRect *rects, int numRects);

#endif
