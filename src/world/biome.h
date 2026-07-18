#pragma once
#include "noise.h"
#include "block_types.h"
#include <cstdint>

namespace Biomes {

enum Biome : uint8_t {
    BIOME_OCEAN,
    BIOME_BEACH,
    BIOME_PLAINS,
    BIOME_FOREST,
    BIOME_DESERT,
    BIOME_SNOWY,
    BIOME_MOUNTAINS,
    BIOME_PINNACLES,
    BIOME_VOLCANIC,
    BIOME_COUNT,
};

struct BiomeInfo {
    const char* name;
    float ambient[3];
    float treeDensity;
};

static const BiomeInfo BIOME_INFO[BIOME_COUNT] = {
    { "Ocean",     {0.45f,0.68f,0.88f}, 0.000f },
    { "Beach",     {0.78f,0.82f,0.84f}, 0.000f },
    { "Plains",    {0.67f,0.79f,0.87f}, 0.012f },
    { "Forest",    {0.52f,0.70f,0.72f}, 0.280f },
    { "Desert",    {0.86f,0.79f,0.57f}, 0.000f },
    { "Snowy",     {0.79f,0.87f,0.96f}, 0.026f },
    { "Mountains", {0.62f,0.72f,0.82f}, 0.004f },
    { "Pinnacles", {0.50f,0.66f,0.76f}, 0.030f },
    { "Volcanic",  {0.33f,0.25f,0.24f}, 0.000f },
};

inline const BiomeInfo& info(Biome b) { return BIOME_INFO[(int)b]; }

struct MacroRegions {
    float forest;
    float frozen;
    float desert;
    float volcano;
};

inline float regionInfluence(float wx, float wz, float cx, float cz, float radius) {
    const float dx = wx - cx;
    const float dz = wz - cz;
    const float distance = sqrtf(dx * dx + dz * dz);
    float t = (distance - radius * 0.42f) / (radius * 0.58f);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return 1.0f - t * t * (3.0f - 2.0f * t);
}

// Compact world: every macro region core sits within ~250 blocks of the
// origin so the player reaches all biomes with a short walk (mobile-first).
// The whole layout — rotation, spacing, jitter — derives from the seed, so
// each seed places its biomes on a genuinely different compass.
inline MacroRegions macroRegions(float wx, float wz, unsigned seed) {
    const float j0 = Noise::value2(0.17f, 0.83f, seed + 3101u);
    const float j1 = Noise::value2(0.71f, 0.29f, seed + 3102u);
    const float j2 = Noise::value2(0.43f, 0.61f, seed + 3103u);
    const float j3 = Noise::value2(0.91f, 0.13f, seed + 3104u);

    const float base = j0 * 6.28318f;                 // seed-rotated compass
    const float js[4] = { j1, j2, j3, j0 };
    float cx[4], cz[4];
    for (int k = 0; k < 4; k++) {
        const float ang  = base + 1.5708f * k + (js[k] - 0.5f) * 0.9f;
        const float dist = 150.0f + js[(k + 1) & 3] * 60.0f;
        cx[k] = cosf(ang) * dist;
        cz[k] = sinf(ang) * dist;
    }
    const float forest  = regionInfluence(wx, wz, cx[0], cz[0], 170.0f);
    const float frozen  = regionInfluence(wx, wz, cx[1], cz[1], 160.0f);
    const float desert  = regionInfluence(wx, wz, cx[2], cz[2], 150.0f);
    const float volcano = regionInfluence(wx, wz, cx[3], cz[3], 110.0f);
    return { forest, frozen, desert, volcano };
}
inline bool specialRegion(float wx, float wz, unsigned seed) {
    float v = 0.5f + 0.5f * Noise::fbm(wx, wz, seed + 770u, 3, 0.0040f, 2.0f, 0.5f);
    return v > 0.70f;
}

inline Biome biomeAt(float wx, float wz, int height, unsigned seed) {
    if (height <= WATER_LEVEL - 2) return BIOME_OCEAN;
    if (height <= WATER_LEVEL + 2) return BIOME_BEACH;

    const MacroRegions regions = macroRegions(wx, wz, seed);
    const float temp = Noise::fbm(wx, wz, seed + 7000u, 2, 0.0032f, 2.0f, 0.5f);
    const float moist = Noise::fbm(wx, wz, seed + 9000u, 2, 0.0032f, 2.0f, 0.5f);

    if (regions.volcano > 0.20f) return BIOME_VOLCANIC;
    if (regions.frozen > 0.28f || temp < -0.30f)
        return height >= 86 ? BIOME_MOUNTAINS : BIOME_SNOWY;
    if (height >= 82 && specialRegion(wx, wz, seed)) return BIOME_PINNACLES;
    if (height >= 86) return BIOME_MOUNTAINS;
    if (regions.desert > 0.24f || (temp > 0.26f && moist < -0.10f)) return BIOME_DESERT;
    if (regions.forest > 0.22f || moist > -0.03f) return BIOME_FOREST;
    return BIOME_PLAINS;
}
inline uint8_t surfaceBlock(Biome b, int height) {
    switch (b) {
        case BIOME_OCEAN:
        case BIOME_BEACH:
        case BIOME_DESERT:    return BLOCK_SAND;
        case BIOME_SNOWY:     return BLOCK_SNOW;
        case BIOME_MOUNTAINS: return (height >= 98) ? BLOCK_SNOW : BLOCK_STONE;
        case BIOME_PINNACLES: return (height >= 96) ? BLOCK_STONE : BLOCK_GRASS;
        case BIOME_VOLCANIC:  return (height >= 82) ? BLOCK_OBSIDIAN : BLOCK_STONE;
        default:              return BLOCK_GRASS;
    }
}

inline uint8_t subBlock(Biome b) {
    switch (b) {
        case BIOME_OCEAN:
        case BIOME_BEACH:
        case BIOME_DESERT:     return BLOCK_SAND;
        case BIOME_MOUNTAINS:
        case BIOME_PINNACLES:
        case BIOME_VOLCANIC: return BLOCK_STONE;
        default:               return BLOCK_DIRT;
    }
}

class BiomeGenerator {
public:
    explicit BiomeGenerator(unsigned seed) : seed_(seed) {}
    Biome at(float wx, float wz, int height) const { return biomeAt(wx, wz, height, seed_); }
    const BiomeInfo& params(Biome b) const { return info(b); }
    uint8_t surface(Biome b, int h) const { return surfaceBlock(b, h); }
    uint8_t sub(Biome b) const { return subBlock(b); }
    unsigned seed() const { return seed_; }
private:
    unsigned seed_;
};

} // namespace Biomes
