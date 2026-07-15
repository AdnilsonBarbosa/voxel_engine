#pragma once
#include "noise.h"
#include "block_types.h"
#include "biome.h"

// Shared world-generation queries used by both terrain and structures, so a
// structure placed near a chunk edge computes the exact same ground height as
// the terrain pass (structures span chunks by being stamped independently from
// each overlapping chunk — they must agree on the heightmap).
namespace WorldGen {

// Raw top-solid-block height for a world column (matches TerrainGen::generate).
inline int terrainHeight(int wx, int wz, unsigned seed) {
    float n = Noise::fbm((float)wx, (float)wz, seed, 6, 0.004f, 2.0f, 0.5f);
    int h = 72 + (int)(n * 24.0f);
    Biomes::Biome b = Biomes::biomeAt((float)wx, (float)wz, h, seed);
    if (b == Biomes::BIOME_MOUNTAINS) {
        float m = Noise::fbm((float)wx, (float)wz, seed + 3u, 4, 0.006f);
        h += (int)((m * 0.5f + 0.5f) * 40.0f);
    }
    if (h < 2)   h = 2;
    if (h > 124) h = 124;
    return h;
}

inline Biomes::Biome biomeAt(int wx, int wz, unsigned seed) {
    return Biomes::biomeAt((float)wx, (float)wz, terrainHeight(wx, wz, seed), seed);
}

} // namespace WorldGen
