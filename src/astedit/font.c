#include <astedit/astedit.h>
#include <astedit/gfx.h>
#include <astedit/draw2d.h>
#include <astedit/textureatlas.h>
#include <astedit/logging.h>
#include <astedit/memoryalloc.h>
#include <astedit/utf8.h>
#include <astedit/font.h>
#include <stdint.h>


struct CachedGlyph {
        struct GlyphMeta meta;  /* used as key */
        struct GlyphLayoutInfo layout;
        struct CachedTexture *cachedTexture;
        struct CachedGlyph *next;  /* hash-chain */
};

enum {
        NUM_HASH_BUCKETS = 1024,  /* change later to be dynamic */
};

static struct CachedGlyph *hash_buckets[NUM_HASH_BUCKETS];


static void init_hash(uint32_t *hsh)
{
        *hsh = 5381;
}

static void update_hash(uint32_t *hsh, const void *ptr, int numBytes)
{
        const unsigned char *x = ptr;
        uint32_t h = *hsh;
        for (int i = 0; i < numBytes; i++)
                h = 33 * h + *x;
        *hsh = h;
}


static uint32_t hash_GlyphMeta(const struct GlyphMeta *meta)
{
        uint32_t hsh;
        init_hash(&hsh);
        update_hash(&hsh, &meta->codepoint, sizeof meta->codepoint);
        update_hash(&hsh, &meta->font, sizeof meta->font);  // XXX we probably don't want to hash a pointer...
        update_hash(&hsh, &meta->size, sizeof meta->size);
        return hsh;
}

static int compare_GlyphMeta(const struct GlyphMeta *x, const struct GlyphMeta *y)
{
        if (x->codepoint != y->codepoint)
                return (x->codepoint > y->codepoint) - (x->codepoint < y->codepoint);
        if (x->font != y->font)
                return (x->font > y->font) - (x->font < y->font);
        if (x->size != y->size)
                return (x->size > y->size) - (x->size < y->size);
        return 0;
}

static struct CachedGlyph *find_cached_glyph(const struct GlyphMeta *meta)
{
        uint32_t hsh = hash_GlyphMeta(meta);
        for (struct CachedGlyph *cachedGlyph = hash_buckets[hsh & (NUM_HASH_BUCKETS - 1)];
                cachedGlyph != NULL; cachedGlyph = cachedGlyph->next)
        {
                if (compare_GlyphMeta(&cachedGlyph->meta, meta) == 0)
                        return cachedGlyph;
        }
        return NULL;
}

static struct CachedGlyph *cache_glyph(const struct GlyphMeta *meta, const struct GlyphLayoutInfo *layout, struct CachedTexture *cachedTexture)
{
        uint32_t hsh = hash_GlyphMeta(meta);
        int pos = hsh & (NUM_HASH_BUCKETS - 1);
        struct CachedGlyph *cachedGlyph;
        ALLOC_MEMORY(&cachedGlyph, 1);
        cachedGlyph->meta = *meta;
        cachedGlyph->layout = *layout;
        cachedGlyph->cachedTexture = cachedTexture;
        cachedGlyph->next = hash_buckets[pos];
        hash_buckets[pos] = cachedGlyph;
        return cachedGlyph;
}

static struct CachedGlyph *render_and_insert_glyph(const struct GlyphMeta *meta)
{
        unsigned char *buffer;
        int stride;
        struct GlyphLayoutInfo layout;
        render_glyph(meta, &buffer, &stride, &layout);

        struct CachedTexture *cachedTexture =
                store_texture_in_texture_atlas(buffer, layout.pixW, layout.pixH, stride);

        struct CachedGlyph *cachedGlyph;
        cachedGlyph = cache_glyph(meta, &layout, cachedTexture);

        return cachedGlyph;
}

static struct CachedGlyph *lookup_or_render_glyph(Font font, int size, uint32_t codepoint)
{
        struct GlyphMeta meta;
        meta.font = font;
        meta.size = size;
        meta.codepoint = codepoint;
        struct CachedGlyph *cachedGlyph = find_cached_glyph(&meta);
        if (cachedGlyph == NULL)
                cachedGlyph = render_and_insert_glyph(&meta);
        return cachedGlyph;
}

int measure_glyph_span(Font font, int size, int cellWidth,
        const uint32_t *text, int length,
        int initX, int *outPositions)
{
        if (!outPositions && cellWidth != -1)
                return length * cellWidth;  // XXX overflow
        int x = initX;
        for (int i = 0; i < length; i++) {
                struct CachedGlyph *cachedGlyph = lookup_or_render_glyph(font, size, text[i]);
                if (outPositions)
                        outPositions[i] = x;
                if (cellWidth == -1)
                        x += cachedGlyph->layout.horiAdvance;
                else
                        x += cellWidth;
        }
        return x;
}

void get_TexDrawInfo_for_glyph(Font font, int size, uint32_t codepoint, struct TexDrawInfo *out)
{
        struct CachedGlyph *cachedGlyph = lookup_or_render_glyph(font, size, codepoint);
        struct TextureAtlasRegion region;
        compute_region_from_CachedTexture(cachedGlyph->cachedTexture, &region);

        out->tex = region.texture;
        out->horiAdvance = cachedGlyph->layout.horiAdvance;
        out->texX = region.texX;
        out->texY = region.texY;
        out->texW = region.texW;
        out->texH = region.texH;
        out->bearingX = cachedGlyph->layout.horiBearingX;
        out->bearingY = cachedGlyph->layout.horiBearingY;
}

void setup_font_atlas(void)
{
        // nothing
}

void teardown_font_atlas(void)
{
        for (int i = 0; i < NUM_HASH_BUCKETS; i++) {
                while (hash_buckets[i]) {
                        struct CachedGlyph *cachedGlyph = hash_buckets[i];
                        hash_buckets[i] = hash_buckets[i]->next;
                        FREE_MEMORY(&cachedGlyph);
                        //log_postf("freed %zu bytes", sizeof *cachedGlyph);
                }
        }
}
