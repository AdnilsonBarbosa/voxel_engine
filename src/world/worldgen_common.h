#pragma once
#include "world_generation_config.h"

namespace WorldGen {

inline Biomes::Biome biomeAt(int wx, int wz, unsigned seed) {
    return Biomes::biomeAt((float)wx, (float)wz,
                           terrainHeight((float)wx, (float)wz, seed), seed);
}

} // namespace WorldGen
