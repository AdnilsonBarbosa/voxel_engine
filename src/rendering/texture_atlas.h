#pragma once
#include "gl_ext.h"
#include <cstdint>

// Procedurally-generated pixel-art texture atlas.
//
// Everything is painted in code at startup — no image files, no external
// assets, zero copyright concerns, tiny RAM. One GL texture holds every block
// face tile, so the whole world still draws with a single texture bind
// (draw-call count is unchanged). Bump TILE_PX to 32/64 for higher-res later.
namespace Atlas {

static constexpr int TILE_PX    = 16;   // pixels per tile (16 → classic pixel art)
static constexpr int ATLAS_COLS = 16;   // tiles per row  → 16*16 = 256 tile slots
static constexpr int ATLAS_PX   = TILE_PX * ATLAS_COLS;

// Tile catalogue. Variants (…0/…1/…2) are consecutive so a material can pick
// one by seed. Keep the count <= ATLAS_COLS*ATLAS_COLS.
enum TileId : uint16_t {
    T_GRASS_TOP0, T_GRASS_TOP1, T_GRASS_TOP2,
    T_GRASS_SIDE,
    T_DIRT0, T_DIRT1, T_DIRT2,
    T_STONE0, T_STONE1, T_STONE2,
    T_COBBLE,
    T_SAND0, T_SAND1,
    T_GRAVEL0, T_GRAVEL1,
    T_BEDROCK,
    T_DEEPSLATE0, T_DEEPSLATE1,
    T_WOOD_SIDE, T_WOOD_TOP,
    T_LEAVES,
    T_SNOW,
    T_ICE,
    T_CLAY,
    T_WATER,
    T_COAL, T_IRON, T_COPPER, T_GOLD, T_DIAMOND, T_EMERALD,
    T_TALL_GRASS, T_FLOWER_RED, T_FLOWER_YELLOW, T_MUSH_RED, T_MUSH_BROWN, T_BUSH,
    T_ROCK,
    T_PLANKS, T_GLASS, T_CHEST_TOP, T_CHEST_SIDE,
    T_TORCH, T_OBSIDIAN,
    T_FARMLAND,
    T_COUNT
};

} // namespace Atlas

class TextureAtlas {
public:
    void init();                    // build pixels + upload GL texture (needs GL context)
    void bind(int unit = 0) const;  // bind atlas to a texture unit
    void cleanup();

    GLuint  texture()  const { return tex; }
    // Normalised tile size and one-texel size (for shader tile wrapping).
    float   tileSizeUV() const { return (float)Atlas::TILE_PX / (float)Atlas::ATLAS_PX; }
    float   texelUV()    const { return 1.0f / (float)Atlas::ATLAS_PX; }
    // Top-left UV of a tile slot.
    void    tileOrigin(uint16_t tile, float& u, float& v) const {
        int col = tile % Atlas::ATLAS_COLS;
        int row = tile / Atlas::ATLAS_COLS;
        u = (float)(col * Atlas::TILE_PX) / (float)Atlas::ATLAS_PX;
        v = (float)(row * Atlas::TILE_PX) / (float)Atlas::ATLAS_PX;
    }

    // Debug
    int     tileCount()   const { return Atlas::T_COUNT; }
    int     atlasPixels() const { return Atlas::ATLAS_PX; }
    long    memoryBytes() const { return (long)Atlas::ATLAS_PX * Atlas::ATLAS_PX * 4; }

private:
    GLuint tex = 0;
};
