#include <astedit/astedit.h>
#include <astedit/window.h>
#include <astedit/logging.h>
#include <astedit/memory.h>
#include <astedit/gfx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <string.h>

extern Display *display;
extern int screen;
extern Window window;
extern XVisualInfo *visualInfo;

Pixmap pixmap;
XImage *image;
GC gc;

static char *pixmapBuffer;
static int pixmapW;
static int pixmapH;
static int viewportX;
static int viewportY;
static int viewportW;
static int viewportH;

enum {
        TEXTURE_RGB,
        TEXTURE_RGBA,
};

struct TextureStruct {
        int textureKind;
        unsigned char *data;
        int w;
        int h;
};

struct TextureStruct *textures;
int numTextures;

static Texture alloc_texture(void)
{
        int idx = numTextures++;
        REALLOC_MEMORY(&textures, numTextures);
        return idx;
}

void clear_screen_and_drawing_state(void)
{
}

void flush_gfx(void)
{
        XFlush(display);
}

void set_2d_coordinate_system(int x, int y, int w, int h)
{
}

void set_viewport_in_pixels(int x, int y, int w, int h)
{
        ENSURE(x >= 0);
        ENSURE(y >= 0);
        ENSURE(x + w <= windowWidthInPixels);
        ENSURE(y + h <= windowHeightInPixels);
        // currently, we use a lazy approach. Instead we should maybe
        // change the buffer when the window size changes
        if (pixmapW != windowWidthInPixels ||
            pixmapH != windowHeightInPixels) {

                if (pixmapBuffer != NULL) {
                        XFreePixmap(display, pixmap);
                }

                pixmapW = windowWidthInPixels;
                pixmapH = windowHeightInPixels;

                pixmap = XCreatePixmap(display, window,
                                       pixmapW,
                                       pixmapH,
                                       24);
                REALLOC_MEMORY(&pixmapBuffer, pixmapH * pixmapW * 4);
        }
        viewportX = x;
        viewportY = y;
        viewportW = w;
        viewportH = h;
}

void set_clipping_rect_in_pixels(int x, int y, int w, int h)
{
}

void clear_clipping_rect(void)
{
}

void gfx_toggle_srgb(void)
{
}

Texture create_rgba_texture(int w, int h)
{
        Texture tex = alloc_texture();
        ALLOC_MEMORY(&textures[tex].data, w * h * 4);
        textures[tex].textureKind = TEXTURE_RGBA;
        textures[tex].w = w;
        textures[tex].h = h;
        return tex;
}

Texture create_rgb_texture(int w, int h)
{
        Texture tex = alloc_texture();
        ALLOC_MEMORY(&textures[tex].data, w * h * 3);
        textures[tex].textureKind = TEXTURE_RGB;
        textures[tex].w = w;
        textures[tex].h = h;
        return tex;
}

/*
Texture create_alpha_texture(int w, int h)
{
        Texture tex = alloc_texture();
        textures[tex].textureKind = TEXTURE_RGBA;
        textures[tex].w = w;
        textures[tex].h = h;
        return tex;
}
*/

void destroy_texture(Texture texHandle)
{
        FREE_MEMORY(&textures[texHandle].data);
        textures[texHandle].w = 0;
        textures[texHandle].h = 0;
}

void update_rgb_texture_subimage(Texture texture, int row, int numRows, int rowWidth, int stride, const unsigned char *data)
{
        ENSURE(rowWidth == 3 * textures[texture].w);
        ENSURE(row + numRows <= textures[texture].h);
        unsigned char *dst = textures[texture].data + row * 3 * textures[texture].w;
        const unsigned char *src = data;
        for (int i = 0; i < numRows; i++) {
                for (int j = 0; j < rowWidth; j++) {
                        *dst++ = *src++;
                }
                src += stride - rowWidth;
        }
}

void draw_glyphs(struct LayedOutGlyph *glyphs, int numGlyphs)
{
        for (int i = 0; i < numGlyphs; i++) {
                struct LayedOutGlyph *glyph = &glyphs[i];
                int tx = glyph->tx;
                int ty = glyph->ty;
                int x = glyph->x + viewportX;
                int y = glyph->y + viewportY;
                int w = glyph->tw;
                int h = glyph->th;
                int colorR = glyph->r;
                int colorG = glyph->g;
                int colorB = glyph->b;
                //int colorA = glyph->a;
                if (x < 0)
                        continue;
                if (x >= viewportX + viewportW)
                        continue;
                if (y < 0)
                        continue;
                if (y >= viewportY + viewportH)
                        continue;
                if (w > viewportX + viewportW - x)
                        w = viewportW - x;
                if (h > viewportY + viewportH - y)
                        h = viewportH - y;
                ENSURE(w >= 0);
                ENSURE(h >= 0);
                ENSURE(0 <= x);
                ENSURE(0 <= y);
                ENSURE(x + w <= pixmapW);
                ENSURE(y + h <= pixmapH);
                ENSURE(textures[glyph->tex].textureKind == TEXTURE_RGB);
                struct TextureStruct *texture = &textures[glyph->tex];
                unsigned char *src = texture->data + 3 * (ty * texture->w + tx);
                int texStride = 3 * texture->w;
                char *ptr = pixmapBuffer + 4 * (y * pixmapW + x);
                for (int j = 0; j < h; j++) {
                        unsigned char *a = ptr;
                        unsigned char *b = src;
                        // TOOD: maybe SIMD this?
                        for (int k = 0; k < w; k++) {
                                a[0] = ((uint32_t) (255 - b[2]) * a[0] + ((uint32_t) b[2] * colorB)) / 255;
                                a[1] = ((uint32_t) (255 - b[1]) * a[1] + ((uint32_t) b[1] * colorG)) / 255;
                                a[2] = ((uint32_t) (255 - b[0]) * a[2] + ((uint32_t) b[0] * colorR)) / 255;
                                a[3] = 0;
                                a += 4;
                                b += 3;
                        }
                        ptr += 4 * pixmapW;
                        src += texStride;
                }
                ENSURE((char*)ptr - pixmapBuffer <= 4 * pixmapW * pixmapH);
        }
}

void draw_rects(struct LayedOutRect *rects, int numRects)
{
        for (int i = 0; i < numRects; i++) {
                struct LayedOutRect *rect = &rects[i];
                int x = rect->x + viewportX;
                int y = rect->y + viewportY;
                int w = rect->w;
                int h = rect->h;
                int r = rect->r;
                int g = rect->g;
                int b = rect->b;
                int a = rect->a;
                if (x < 0)
                        continue;
                if (x >= viewportX + viewportW)
                        continue;
                if (y < 0)
                        continue;
                if (y >= viewportY + viewportH)
                        continue;
                if (w > viewportX + viewportW - x)
                        w = viewportW - x;
                if (h > viewportY + viewportH - y)
                        h = viewportH - y;
                uint32_t *ptr = pixmapBuffer + 4 * (y * pixmapW + x);
                uint32_t color = (a << 24) | (r << 16) | (g << 8) | b;
                for (int j = 0; j < h; j++) {
                        uint32_t *row = ptr;
                        for (int k = 0; k < w; k++)
                                row[k] = color;
                        ptr += pixmapW;
                }
        }
}

void swap_buffers(void)
{
        /* That "API" is really unconventional in that we
         * have to set the values, and we have to unset them
         * bufore calling XDestroyImage() or otherwise it will
         * try to free() that data pointer. */
        image->data = (char *)pixmapBuffer;
        image->width = pixmapW;
        image->height = pixmapH;
        image->bytes_per_line = 4 * pixmapW;
        image->bits_per_pixel = 32;
        XPutImage(display, window, gc, image, 0, 0, 0, 0, pixmapW, pixmapH);
        image->data = NULL;
        image->width = 0;
        image->height = 0;
}

void setup_gfx(void)
{
        gc = DefaultGC(display, screen);
        image = XCreateImage(display, visualInfo->visual, 24,
                             ZPixmap, 0,
                             NULL, 0, 0, /* data, width, height only set when copying */
                             32, 0);
}


void teardown_gfx(void)
{
        // I think that GC must not be freed when allocated with DefaultGC
        XDestroyImage(image);
        if (pixmapBuffer) {
                XFreePixmap(display, pixmap);
                FREE_MEMORY(&pixmapBuffer);
        }
}

