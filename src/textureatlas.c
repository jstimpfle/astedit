#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/logging.h>
#include <astedit/memoryalloc.h>
#include <astedit/textureatlas.h>

enum {
        ATLASTEXTURE_WIDTH = 1024,
        ATLASTEXTURE_HEIGHT = 1024,
};

struct AtlasTexture;

struct AtlasTextureRow {
        struct AtlasTexture *atlasTexture;
        int height;  /* height in pixels */
        int width;  /* width in pixels */
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




static void debug_print_texture(unsigned char *pixels, int pixW, int pixH, int stride)
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



/* TODO: more efficient allocation */
static struct CachedTexture *alloc_CachedTexture(void)
{
        struct CachedTexture *out;
        ALLOC_MEMORY(&out, 1);
        return out;
}

static struct AtlasTexture *alloc_AtlasTexture(void)
{
        /* create a new AtlasTexture and allocate a row in it */
        int texIdx = numAtlasTextures++;
        REALLOC_MEMORY(&atlasTextures, numAtlasTextures);
        ALLOC_MEMORY(&atlasTextures[texIdx], 1);
        return atlasTextures[texIdx];
}

static struct AtlasTextureRow *alloc_row(struct AtlasTexture *a, int y, int w, int h)
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
        row->width = ATLASTEXTURE_WIDTH;
        row->dirty = 0;
        ALLOC_MEMORY(&row->buffer, h * ATLASTEXTURE_WIDTH);
        return row;
}


void copy_texture(unsigned char *dstBuffer, unsigned char *srcBuffer,
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


void reset_texture_atlas(void)
{
        /*TODO*/
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
                        if (row->height == h && row->width - row->x >= w)
                                return row;
                }                
        }

        /* look for an AtlasTexture that has enough space for a new row */
        for (int i = 0; i < numAtlasTextures; i++) {
                struct AtlasTexture *a = atlasTextures[i];
                int y = 0;
                y = 28; //XXX for testing: don't start using the texture at the top
                if (a->numRows > 0) {
                        struct AtlasTextureRow *row = a->rows[a->numRows - 1];
                        y = row->y + row->height;
                }
                if (a->height - y >= h) {
                        return alloc_row(a, y, w, h);
                }
        }

        struct AtlasTexture *a = alloc_AtlasTexture();
        a->alphaTexture = create_alpha_texture(ATLASTEXTURE_WIDTH, ATLASTEXTURE_HEIGHT);
        a->width = ATLASTEXTURE_WIDTH;
        a->height = ATLASTEXTURE_HEIGHT;
        a->numRows = 0;
        a->rows = NULL;
        return alloc_row(a, 28/*XXX same as above*/, a->width, h);
}


struct CachedTexture *store_texture_in_texture_atlas(unsigned char *pixels, int pixW, int pixH, int stride)
{
        //debug_print_texture(pixels, pixH, pixW, stride);

        int h = (pixH + 3) / 4 * 4;  /* next multiple of 4 */
        
        struct AtlasTextureRow *row = find_or_alloc_row(pixW, h);
        int pixX = row->x;
        row->x += pixW;

        copy_texture(row->buffer + pixX, pixels, pixW, pixH, row->width, stride);

        row->dirty = 1;
        
        struct CachedTexture *out = alloc_CachedTexture();
        out->row = row;
        out->x = pixX;
        out->width = pixW;
        out->height = pixH;

        return out;
}

void compute_region_from_CachedTexture(struct CachedTexture *cachedTexture, struct TextureAtlasRegion *outRegion)
{
        outRegion->texture = cachedTexture->row->atlasTexture->alphaTexture;
        outRegion->texX = (float) cachedTexture->x / ATLASTEXTURE_WIDTH;
        outRegion->texY = (float) cachedTexture->row->y / ATLASTEXTURE_HEIGHT;
        outRegion->texW = (float) cachedTexture->width / ATLASTEXTURE_WIDTH;
        outRegion->texH = (float) cachedTexture->height / ATLASTEXTURE_HEIGHT;
}

void commit_all_dirty_textures(void)
{
        for (int i = 0; i < numAtlasTextures; i++) {
                struct AtlasTexture *a = atlasTextures[i];
                for (int j = 0; j < a->numRows; j++) {
                        struct AtlasTextureRow *row = a->rows[j];
                        if (row->dirty) {
                                ENSURE(row->buffer != NULL);
                                update_alpha_texture_subimage(a->alphaTexture, row->y, row->height, row->width, row->width, row->buffer);
                                row->dirty = 0;
                                //FREE_MEMORY(&row->buffer);
                                //ENSURE(row->buffer == NULL);
                        }
                }
        }
}