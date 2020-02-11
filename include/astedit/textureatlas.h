#include <astedit/astedit.h>
#include <astedit/gfx.h>
#include <astedit/font.h>

struct CachedTexture;

struct TextureAtlasRegion {
        Texture texture;
        int texX;
        int texY;
        int texW;
        int texH;
};

struct CachedTexture *store_texture_in_texture_atlas(unsigned char *pixels, int pixW, int pixH, int stride);
void compute_region_from_CachedTexture(struct CachedTexture *cachedTexture, struct TextureAtlasRegion *outRegion);
void commit_all_dirty_textures(void);
void setup_texture_atlas(void);
void teardown_texture_atlas(void);
