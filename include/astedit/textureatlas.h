#include <astedit/astedit.h>
#include <astedit/gfx.h>
#include <astedit/font.h>

struct CachedTexture;

struct TextureAtlasRegion {
        Texture texture;
        float texX;
        float texY;
        float texW;
        float texH;
};

void reset_texture_atlas(void);

struct CachedTexture *store_texture_in_texture_atlas(unsigned char *pixels, int pixW, int pixH, int stride);

void compute_region_from_CachedTexture(struct CachedTexture *cachedTexture, struct TextureAtlasRegion *outRegion);

void commit_all_dirty_textures(void);
