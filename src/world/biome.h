#pragma once
#include "noise.h"
#include "block_types.h"
#include <cstdint>

// Procedural biome selection driven purely by world seed + position.
// Temperature/humidity are low-frequency noise fields so biomes form
// large contiguous regions instead of per-block noise.
namespace Biomes {

enum Biome : uint8_t {
    BIOME_OCEAN,
    BIOME_BEACH,
    BIOME_PLAINS,
    BIOME_FOREST,
    BIOME_DESERT,
    BIOME_SNOWY,
    BIOME_MOUNTAINS,
    BIOME_COUNT,
};

// Per-biome parameters. `ambient` is the environment/fog tint used to colour
// the sky and distance haze when the player is inside this biome.
struct BiomeInfo {
    const char* name;
    float ambient[3];    // RGB environment colour
    float treeDensity;   // probability of a tree per surface column
};

static const BiomeInfo BIOME_INFO[BIOME_COUNT] = {
    { "Ocean",     {0.55f,0.72f,0.90f}, 0.000f },
    { "Beach",     {0.80f,0.84f,0.86f}, 0.000f },
    { "Plains",    {0.70f,0.82f,0.92f}, 0.008f },
    { "Forest",    {0.60f,0.76f,0.78f}, 0.055f },
    { "Desert",    {0.86f,0.80f,0.60f}, 0.000f },
    { "Snowy",     {0.82f,0.88f,0.96f}, 0.022f },
    { "Mountains", {0.70f,0.75f,0.83f}, 0.004f },
};

inline const BiomeInfo& info(Biome b) { return BIOME_INFO[b]; }

inline Biome biomeAt(float wx, float wz, int height, unsigned seed) {
    if (height <= WATER_LEVEL - 1)  return BIOME_OCEAN;
    if (height <= WATER_LEVEL + 1)  return BIOME_BEACH;
    if (height >= 84)               return BIOME_MOUNTAINS;

    // ~0.004 → biome regions ~250 blocks across: large and contiguous, but
    // small enough that the player crosses several while exploring.
    float temp  = Noise::fbm(wx, wz, seed + 7000u, 3, 0.004f);
    float moist = Noise::fbm(wx, wz, seed + 9000u, 3, 0.004f);

    if (temp < -0.30f)                    return BIOME_SNOWY;
    if (temp >  0.32f && moist < -0.05f)  return BIOME_DESERT;
    if (moist >  0.15f)                   return BIOME_FOREST;
    return BIOME_PLAINS;
}

// Top block of the column for this biome.
inline uint8_t surfaceBlock(Biome b, int height) {
    switch (b) {
        case BIOME_OCEAN:     return BLOCK_SAND;
        case BIOME_BEACH:     return BLOCK_SAND;
        case BIOME_DESERT:    return BLOCK_SAND;
        case BIOME_SNOWY:     return BLOCK_SNOW;
        case BIOME_MOUNTAINS: return (height >= 96) ? BLOCK_SNOW : BLOCK_STONE;
        default:              return BLOCK_GRASS;   // plains / forest
    }
}

// The few blocks directly beneath the surface.
inline uint8_t subBlock(Biome b) {
    switch (b) {
        case BIOME_OCEAN:
        case BIOME_BEACH:
        case BIOME_DESERT:    return BLOCK_SAND;
        case BIOME_MOUNTAINS: return BLOCK_STONE;
        default:              return BLOCK_DIRT;
    }
}

// Thin wrapper class — a single object that owns the seed and produces the
// biome + its parameters for any world coordinate. Backed by the project's
// value-noise FBM (drop-in replaceable with FastNoiseLite if desired).
class BiomeGenerator {
public:
    explicit BiomeGenerator(unsigned seed) : seed_(seed) {}

    Biome at(float wx, float wz, int height) const {
        return biomeAt(wx, wz, height, seed_);
    }
    const BiomeInfo& params(Biome b)   const { return info(b); }
    uint8_t surface(Biome b, int h)    const { return surfaceBlock(b, h); }
    uint8_t sub(Biome b)               const { return subBlock(b); }
    unsigned seed()                    const { return seed_; }

private:
    unsigned seed_;
};

} // namespace Biomes
