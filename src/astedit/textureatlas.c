#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/logging.h>
#include <astedit/memory.h>
#include <astedit/textureatlas.h>

int atlasTextureBytesAllocated;
int atlasTextureBytesUsed;

enum {
        /* the actual numbers of pixels stored is ATLASTEXTURE_WIDTH / 3. (RGB
         * format) */
        ATLASTEXTURE_WIDTH = 3 * 1024,  /* must be multiple of 4 */
        ATLASTEXTURE_HEIGHT = 1024,
};

struct AtlasTexture;

struct AtlasTextureRow {
        struct AtlasTexture *atlasTexture;
        int height;  /* height in pixels */
        int x;  /* texture is used up from left to right. `x` indicates the x-offset of the first unused pixel */
        int y;  /* offset in texture */
        int dirty;
        unsigned char *buffer;  /* width * height pixels. NULL if not dirty */
};

struct AtlasTexture {
        Texture alphaTexture;
        int width;
        int height;
        int numRows;
        struct AtlasTextureRow **rows;
};


struct CachedTexture {
        struct AtlasTextureRow *row;
        int x;
        int width;
        int height;
};


static struct AtlasTexture **atlasTextures;
static int numAtlasTextures;


static UNUSEDFUNC void debug_print_texture(unsigned char *pixels, int pixW, int pixH, int stride)
{
        log_begin();
        int pos = 0;
        for (int i = 0; i < pixH; i++) {
                for (int j = 0; j < pixW; j++) {
                        int c = pixels[pos + j];
                        if (c < 128)
                                log_write(" ", 1);
                        else
                                log_write("X", 1);
                }
                log_write("\n", 1);
                pos += stride;
        }
        log_end();
}


static struct AtlasTexture *alloc_AtlasTexture(void)
{
        /* create a new AtlasTexture and allocate a row in it */
        int texIdx = numAtlasTextures++;
        REALLOC_MEMORY(&atlasTextures, numAtlasTextures);
        ALLOC_MEMORY(&atlasTextures[texIdx], 1);
        struct AtlasTexture *a = atlasTextures[texIdx];
        atlasTextureBytesAllocated += ATLASTEXTURE_WIDTH * ATLASTEXTURE_HEIGHT;
        a->alphaTexture = create_rgb_texture(ATLASTEXTURE_WIDTH / 3, ATLASTEXTURE_HEIGHT);
        a->width = ATLASTEXTURE_WIDTH;
        a->height = ATLASTEXTURE_HEIGHT;
        a->numRows = 0;
        a->rows = NULL;
        return a;
}

static struct AtlasTextureRow *alloc_row(struct AtlasTexture *a, int y, int h)
{
        ENSURE(h <= ATLASTEXTURE_HEIGHT - y);
        int index = a->numRows++;
        REALLOC_MEMORY(&a->rows, a->numRows);
        ALLOC_MEMORY(&a->rows[index], 1);
        struct AtlasTextureRow *row = a->rows[index];
        row->atlasTexture = a;
        row->x = 0;
        row->y = y;
        row->height = h;
        row->dirty = 0;
        ALLOC_MEMORY(&row->buffer, ATLASTEXTURE_WIDTH * row->height);
        return row;
}

static void copy_texture(unsigned char *dstBuffer, unsigned char *srcBuffer,
        int width, int height, int dstStride, int srcStride)
{
        int dstpos = 0;
        int srcpos = 0;
        for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++)
                        dstBuffer[dstpos + j] = srcBuffer[srcpos + j];
                dstpos += dstStride;
                srcpos += srcStride;
        }
        //debug_print_texture(dstBuffer, width, height, dstStride);
}

static struct AtlasTextureRow *find_or_alloc_row(int w, int h)
{
        ENSURE(w <= ATLASTEXTURE_WIDTH);
        ENSURE(h <= ATLASTEXTURE_HEIGHT);

        /* look for a row that has enough space in it */
        for (int i = 0; i < numAtlasTextures; i++) {
                struct AtlasTexture *a = atlasTextures[i];
                for (int j = 0; j < a->numRows; j++) {
                        struct AtlasTextureRow *row = a->rows[j];
                        if (row->height == h && ATLASTEXTURE_WIDTH - row->x >= w)
                                return row;
                }
        }

        /* look for an AtlasTexture that has enough space for a new row */
        struct AtlasTexture *atlas = NULL;
        int y = 0;
        for (int i = 0; i < numAtlasTextures; i++) {
                struct AtlasTexture *a = atlasTextures[i];
                if (a->numRows == 0)
                        y = 0;
                else /* a->numRows > 0*/ {
                        struct AtlasTextureRow *row = a->rows[a->numRows - 1];
                        y = row->y + row->height;
                }
                if (a->height - y >= h) {
                        atlas = a;
                        break;
                }
        }
        if (atlas == NULL) {
                atlas = alloc_AtlasTexture();
                y = 0;
        }
        return alloc_row(atlas, y, h);
}

struct CachedTexture *store_texture_in_texture_atlas(unsigned char *pixels, int pixW, int pixH, int stride)
{
        //debug_print_texture(pixels, pixW, pixH, stride);

        /* dimensions of the subimage that we want to allocate in atlas */
        int w = pixW;
        int h = (pixH + 3) & ~3;  /* next multiple of 4 */

        struct AtlasTextureRow *row = find_or_alloc_row(w, h);
        atlasTextureBytesUsed += w * h;
        int pixX = row->x;
        row->x += pixW;

        int srcStride = stride;
        int dstStride = ATLASTEXTURE_WIDTH;

        copy_texture(row->buffer + pixX, pixels, pixW, pixH, dstStride, srcStride);
        row->dirty = 1;

        struct CachedTexture *cachedTexture;
        // XXX this is currently a leak
        ALLOC_MEMORY(&cachedTexture, 1);
        cachedTexture->row = row;
        cachedTexture->x = pixX;
        cachedTexture->width = pixW;
        cachedTexture->height = pixH;
        return cachedTexture;
}

void compute_region_from_CachedTexture(struct CachedTexture *cachedTexture, struct TextureAtlasRegion *outRegion)
{
        outRegion->texture = cachedTexture->row->atlasTexture->alphaTexture;
        outRegion->texX = cachedTexture->x / 3;  /* Divide by 3: RGB format */
        outRegion->texY = cachedTexture->row->y;
        outRegion->texW = cachedTexture->width / 3;  /* Divide by 3: RGB format */
        outRegion->texH = cachedTexture->height;
}

void commit_all_dirty_textures(void)
{
        for (int i = 0; i < numAtlasTextures; i++) {
                struct AtlasTexture *a = atlasTextures[i];
                for (int j = 0; j < a->numRows; j++) {
                        struct AtlasTextureRow *row = a->rows[j];
                        if (row->dirty) {
                                update_rgb_texture_subimage(a->alphaTexture, row->y, row->height, ATLASTEXTURE_WIDTH, ATLASTEXTURE_WIDTH, row->buffer);
                                row->dirty = 0;
                        }
                }
        }
}

void setup_texture_atlas(void)
{
        /*TODO*/
}

void teardown_texture_atlas(void)
{
        for (int i = 0; i < numAtlasTextures; i++) {
                struct AtlasTexture *a = atlasTextures[i];
                for (int j = 0; j < a->numRows; j++) {
                        FREE_MEMORY(&a->rows[j]->buffer);
                        FREE_MEMORY(&a->rows[j]);
                }
                destroy_texture(a->alphaTexture);
                FREE_MEMORY(&a->rows);
                FREE_MEMORY(&atlasTextures[i]);
        }
        FREE_MEMORY(&atlasTextures);
}
