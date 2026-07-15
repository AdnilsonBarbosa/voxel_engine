#pragma once
#include "noise.h"
#include "block_types.h"
#include <cstdint>

// OreGenerator — deterministic, seed-driven ore veins baked as blocks inside
// the chunk (no per-ore objects). Each ore has a depth band, a rarity cutoff
// and a vein scale. Sampled with 3D value noise so veins form compact blobs.
namespace Ores {

struct OreDef {
    uint8_t     block;
    int         minY, maxY;   // depth band (world Y) where this ore may spawn
    float       threshold;    // vein noise cutoff in [-1,1] — higher = rarer
    float       scale;        // vein noise frequency — higher = smaller veins
    const char* name;
};

// Rarest / deepest first: the first matching ore wins, so diamond/gold take
// priority over coal in the depth ranges where they overlap.
static const OreDef ORES[] = {
    { BLOCK_DIAMOND, 1,   16,  0.86f, 0.14f, "Diamond" },
    { BLOCK_EMERALD, 4,   40,  0.90f, 0.17f, "Emerald" },
    { BLOCK_GOLD,    1,   30,  0.84f, 0.13f, "Gold"    },
    { BLOCK_IRON,    4,   64,  0.80f, 0.11f, "Iron"    },
    { BLOCK_COPPER,  30,  84,  0.80f, 0.11f, "Copper"  },
    { BLOCK_COAL,    16,  110, 0.76f, 0.10f, "Coal"    },
};
static const int ORE_COUNT = 6;

// Ore block for this position, or 0 (BLOCK_AIR) to keep the host stone.
inline uint8_t at(int wx, int y, int wz, unsigned seed) {
    for (int i = 0; i < ORE_COUNT; i++) {
        const OreDef& o = ORES[i];
        if (y < o.minY || y > o.maxY) continue;
        float n = Noise::fbm3((float)wx, (float)y, (float)wz,
                              seed + 500u + (unsigned)i * 97u, 2, o.scale);
        if (n > o.threshold) return o.block;
    }
    return 0;
}

// Index of an ore block within ORES, or -1 if not an ore.
inline int indexOf(uint8_t block) {
    for (int i = 0; i < ORE_COUNT; i++)
        if (ORES[i].block == block) return i;
    return -1;
}

} // namespace Ores
