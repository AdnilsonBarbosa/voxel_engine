#pragma once
#include "chunk.h"
#include "block_types.h"
#include "biome.h"
#include "world_generation_config.h"
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace Decoration {

inline uint32_t hash2(int x, int z, unsigned s) {
    uint32_t n = (uint32_t)(x * 374761393u ^ z * 668265263u ^ s * 2246822519u);
    n ^= n >> 13; n *= 1274126177u; n ^= n >> 16;
    return n;
}

inline float rnd(int x, int z, unsigned s) {
    return (hash2(x, z, s) & 0xFFFFFF) / (float)0x1000000;
}

inline void setIfAir(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                     int x, int y, int z, uint8_t type) {
    if (x < 0 || x >= CHUNK_W || y < 0 || y >= CHUNK_H ||
        z < 0 || z >= CHUNK_D) return;
    if (b[x][y][z] == BLOCK_AIR) b[x][y][z] = type;
}

// ── Tree species ─────────────────────────────────────────────────────────────
enum TreeSpecies : uint8_t {
    SP_OAK,      // round layered canopy, thick trunk, branches
    SP_BIRCH,    // tall slim white trunk, small light-green crown
    SP_PINE,     // conical stacked rings of dark needles
    SP_AUTUMN,   // oak-shaped orange canopy
    SP_CYPRESS,  // narrow dark column
    SP_DEAD,     // bare trunk with stub branches
    SP_MANGROVE, // stilt roots in the shallows, wide flat canopy, hanging lianas
};

struct TreeShape {
    TreeSpecies species;
    int trunkHeight;   // wood column above surface
    int maxRadius;     // widest canopy radius (clearance)
    int topPad;        // canopy blocks above trunk top (clearance)
    int lean;          // ±1: upper trunk shifts one block sideways
    float density;     // per-tree leaf density (age/health)
};

inline bool isTreeBlock(uint8_t t) {
    return t == BLOCK_WOOD || t == BLOCK_WOOD_BIRCH ||
           t == BLOCK_LEAVES || t == BLOCK_LEAVES_PINE ||
           t == BLOCK_LEAVES_BIRCH || t == BLOCK_LEAVES_AUTUMN ||
           t == BLOCK_VINE;
}

// Like setIfAir but also claims still water — mangrove roots and trunks
// rise through the shallows.
inline void setIfSoft(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                      int x, int y, int z, uint8_t type) {
    if (x < 0 || x >= CHUNK_W || y < 0 || y >= CHUNK_H ||
        z < 0 || z >= CHUNK_D) return;
    if (b[x][y][z] == BLOCK_AIR || b[x][y][z] == BLOCK_WATER)
        b[x][y][z] = type;
}

// Every tree draws its own age, vigour and posture from position hashes, so
// species × growth stage × continuous jitter means no two trees match.
inline TreeShape shapeFor(Biomes::Biome biome, int wx, int wz, unsigned seed) {
    const float r = rnd(wx, wz, seed + 1201u);
    const int size = (int)(rnd(wx, wz, seed + 1303u) * 3.0f); // 0..2 growth stage
    const float age    = rnd(wx, wz, seed + 1409u);           // continuous jitter
    const float vigour = rnd(wx, wz, seed + 1511u);
    TreeSpecies sp;

    if (biome == Biomes::BIOME_SNOWY)          sp = r < 0.92f ? SP_PINE : SP_DEAD;
    else if (biome == Biomes::BIOME_MOUNTAINS) sp = r < 0.84f ? SP_PINE : SP_DEAD;
    else if (biome == Biomes::BIOME_PINNACLES) sp = r < 0.50f ? SP_PINE
                                                  : r < 0.86f ? SP_CYPRESS : SP_DEAD;
    else if (biome == Biomes::BIOME_PLAINS) {
        sp = r < 0.58f ? SP_OAK : r < 0.82f ? SP_BIRCH
           : r < 0.94f ? SP_AUTUMN : SP_DEAD;
    } else { // forest and everything else
        sp = r < 0.36f ? SP_OAK    : r < 0.58f ? SP_BIRCH
           : r < 0.74f ? SP_AUTUMN : r < 0.88f ? SP_PINE
           : r < 0.96f ? SP_CYPRESS : SP_DEAD;
    }

    const int   jitterH  = (int)(age * 3.0f);                 // +0..2 height
    const float density  = 0.84f + 0.13f * vigour;            // sparse..lush
    // Old broadleaves and dead trees occasionally lean.
    const int lean = (sp == SP_OAK || sp == SP_AUTUMN || sp == SP_DEAD) &&
                     age > 0.82f ? (vigour > 0.5f ? 1 : -1) : 0;

    switch (sp) {
        case SP_OAK:     return { sp, 5 + size * 2 + jitterH,
                                  2 + (size > 0 ? 1 : 0), 2, lean, density };
        case SP_BIRCH:   return { sp, 7 + size * 2 + jitterH, 2, 2, 0, density };
        case SP_PINE:    return { sp, 8 + size * 3 + jitterH, 3, 2, 0, density };
        case SP_AUTUMN:  return { sp, 5 + size + jitterH,
                                  2 + (size > 1 ? 1 : 0), 2, lean, density };
        case SP_CYPRESS: return { sp, 6 + size * 2 + jitterH, 1, 3, 0, density };
        default:         return { SP_DEAD, 5 + size + jitterH, 2, 1, lean, 0.0f };
    }
}

// Mangroves grow from lake and river beds, so their shape comes from the
// water depth at the cell, not from the biome table.
inline TreeShape mangroveShape(int wx, int wz, unsigned seed) {
    const int   size   = (int)(rnd(wx, wz, seed + 1303u) * 3.0f);
    const float age    = rnd(wx, wz, seed + 1409u);
    const float vigour = rnd(wx, wz, seed + 1511u);
    return { SP_MANGROVE, 8 + size * 2 + (int)(age * 3.0f), 3, 2, 0,
             0.86f + 0.12f * vigour };
}

inline bool clearTreeVolume(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                            int lx, int surfY, int lz,
                            const TreeShape& s) {
    // Dense forests allow crowns to overlap; only trunks need clearance.
    const int extent = s.maxRadius >= 3 ? 3 : 2;
    if (lx - extent < 0 || lx + extent >= CHUNK_W ||
        lz - extent < 0 || lz + extent >= CHUNK_D) return false;

    const int top = surfY + s.trunkHeight + s.topPad;
    for (int y = surfY + 1; y <= top && y < CHUNK_H; y++) {
        // Mangrove trunks may rise through still water; everyone else
        // needs clear air above the surface block.
        const uint8_t centre = b[lx][y][lz];
        if (centre != BLOCK_AIR &&
            !(s.species == SP_MANGROVE && centre == BLOCK_WATER &&
              y <= WATER_LEVEL)) return false;
        for (int x = lx - extent; x <= lx + extent; x++)
        for (int z = lz - extent; z <= lz + extent; z++)
            if (isTreeBlock(b[x][y][z])) return false;
    }
    return top < CHUNK_H;
}

inline void leafBlob(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                     int cx, int cy, int cz, int radius,
                     float density, uint8_t leaf, unsigned seed) {
    for (int dx = -radius; dx <= radius; dx++)
    for (int dy = -radius; dy <= radius; dy++)
    for (int dz = -radius; dz <= radius; dz++) {
        const float vertical = (float)dy / (float)(radius + 1);
        const float dist = (float)(dx * dx + dz * dz) +
                           vertical * vertical * (float)(radius * radius);
        if (dist > (float)(radius * radius + 1)) continue;
        if (rnd(cx + dx * 11 + dz, cz + dz * 7 + dy, seed) > density) continue;
        setIfAir(b, cx + dx, cy + dy, cz + dz, leaf);
    }
}

// Flat horizontal disc of leaves — one pine/cypress canopy layer.
inline void leafDisc(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                     int cx, int cy, int cz, int radius,
                     float density, uint8_t leaf, unsigned seed) {
    for (int dx = -radius; dx <= radius; dx++)
    for (int dz = -radius; dz <= radius; dz++) {
        if (dx * dx + dz * dz > radius * radius + 1) continue;
        if (radius > 0 && rnd(cx + dx * 13, cz + dz * 17, seed) > density) continue;
        setIfAir(b, cx + dx, cy, cz + dz, leaf);
    }
}

// Trunk with optional lean: the upper half steps one block sideways, and the
// builder receives the crown centre back so the canopy follows the posture.
inline void trunk(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                  int lx, int surfY, int lz, int height, int lean,
                  uint8_t wood, int& topX) {
    topX = lx;
    for (int y = 1; y <= height; y++) {
        if (lean && y > height / 2) topX = lx + lean;
        setIfAir(b, topX, surfY + y, lz, wood);
    }
}

inline void buildOak(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                     int lx, int surfY, int lz,
                     const TreeShape& s, uint8_t leaf, unsigned seed) {
    int tx;
    trunk(b, lx, surfY, lz, s.trunkHeight, s.lean, BLOCK_WOOD, tx);
    const int topY = surfY + s.trunkHeight;
    // Big oaks grow four short branches under the crown.
    if (s.trunkHeight >= 8) {
        setIfAir(b, tx + 1, topY - 2, lz, BLOCK_WOOD);
        setIfAir(b, tx - 1, topY - 2, lz, BLOCK_WOOD);
        setIfAir(b, tx, topY - 2, lz + 1, BLOCK_WOOD);
        setIfAir(b, tx, topY - 2, lz - 1, BLOCK_WOOD);
    }
    leafBlob(b, tx, topY, lz, s.maxRadius, s.density + 0.05f, leaf, seed + 101u);
    leafBlob(b, tx, topY - 2, lz, s.maxRadius - 1, s.density, leaf, seed + 107u);
    leafBlob(b, tx, topY + 1, lz, s.maxRadius - 1, s.density - 0.03f, leaf, seed + 113u);
}

inline void buildBirch(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                       int lx, int surfY, int lz,
                       const TreeShape& s, unsigned seed) {
    int tx;
    trunk(b, lx, surfY, lz, s.trunkHeight, 0, BLOCK_WOOD_BIRCH, tx);
    const int topY = surfY + s.trunkHeight;
    leafBlob(b, tx, topY, lz, 2, s.density + 0.02f, BLOCK_LEAVES_BIRCH, seed + 101u);
    leafBlob(b, tx, topY - 2, lz, 1, s.density - 0.02f, BLOCK_LEAVES_BIRCH, seed + 107u);
    setIfAir(b, tx, topY + 2, lz, BLOCK_LEAVES_BIRCH);
}

inline void buildPine(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                      int lx, int surfY, int lz,
                      const TreeShape& s, unsigned seed) {
    int tx;
    trunk(b, lx, surfY, lz, s.trunkHeight, 0, BLOCK_WOOD, tx);
    // Cone: rings shrink from the lowest canopy layer up to the tip. Canopy
    // start varies per tree (bare-trunked old pines vs bushy young ones).
    const int skirt = 2 + (int)(rnd(lx * 3 + lz, lz * 5 - lx, seed + 61u) * 2.0f);
    const int canopyBase = surfY + (s.trunkHeight * skirt) / (skirt + 3);
    const int topY = surfY + s.trunkHeight;
    for (int y = canopyBase; y <= topY; y++) {
        const float t = (float)(topY - y) / (float)(topY - canopyBase + 1);
        int radius = 1 + (int)(t * (float)s.maxRadius);
        if (((y - canopyBase) & 1) == 0) radius = radius > 1 ? radius - 1 : 1;
        leafDisc(b, tx, y, lz, radius, s.density + 0.06f,
                 BLOCK_LEAVES_PINE, seed + (unsigned)y);
    }
    setIfAir(b, tx, topY + 1, lz, BLOCK_LEAVES_PINE);
    setIfAir(b, tx, topY + 2, lz, BLOCK_LEAVES_PINE);
}

inline void buildCypress(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                         int lx, int surfY, int lz,
                         const TreeShape& s, unsigned seed) {
    int tx;
    trunk(b, lx, surfY, lz, 1, 0, BLOCK_WOOD, tx);
    // Tight leaf column with a pointed tip.
    const int topY = surfY + s.trunkHeight;
    for (int y = surfY + 2; y <= topY; y++)
        leafDisc(b, tx, y, lz, 1, s.density + 0.08f,
                 BLOCK_LEAVES_PINE, seed + (unsigned)y);
    setIfAir(b, tx, topY + 1, lz, BLOCK_LEAVES_PINE);
    setIfAir(b, tx, topY + 2, lz, BLOCK_LEAVES_PINE);
}

inline void buildDead(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                      int lx, int surfY, int lz,
                      const TreeShape& s, unsigned seed) {
    int tx;
    trunk(b, lx, surfY, lz, s.trunkHeight, s.lean, BLOCK_WOOD, tx);
    const int topY = surfY + s.trunkHeight;
    if (rnd(lx, lz, seed + 31u) < 0.5f) setIfAir(b, tx + 1, topY - 1, lz, BLOCK_WOOD);
    if (rnd(lx, lz, seed + 37u) < 0.5f) setIfAir(b, tx - 1, topY - 2, lz, BLOCK_WOOD);
    if (rnd(lx, lz, seed + 41u) < 0.5f) setIfAir(b, tx, topY - 1, lz + 1, BLOCK_WOOD);
}

inline void buildMangrove(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                          int lx, int surfY, int lz,
                          const TreeShape& s, unsigned seed) {
    // Stilt roots: four legs one block out from the trunk, rising from the
    // bed through the shallows and joining the trunk above the waterline.
    static const int LEG[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
    for (int i = 0; i < 4; i++) {
        setIfSoft(b, lx + LEG[i][0], surfY + 1, lz + LEG[i][1], BLOCK_WOOD);
        setIfSoft(b, lx + LEG[i][0], surfY + 2, lz + LEG[i][1], BLOCK_WOOD);
    }
    for (int y = 1; y <= s.trunkHeight; y++)
        setIfSoft(b, lx, surfY + y, lz, BLOCK_WOOD);

    // Wide, flat, layered canopy — the umbrella the lianas hang from.
    const int topY = surfY + s.trunkHeight;
    setIfAir(b, lx + 1, topY - 2, lz, BLOCK_WOOD);   // short branches
    setIfAir(b, lx - 1, topY - 2, lz, BLOCK_WOOD);
    setIfAir(b, lx, topY - 2, lz + 1, BLOCK_WOOD);
    setIfAir(b, lx, topY - 2, lz - 1, BLOCK_WOOD);
    leafDisc(b, lx, topY - 1, lz, s.maxRadius,     s.density + 0.10f,
             BLOCK_LEAVES, seed + 151u);
    leafDisc(b, lx, topY,     lz, s.maxRadius,     s.density + 0.06f,
             BLOCK_LEAVES, seed + 157u);
    leafDisc(b, lx, topY + 1, lz, s.maxRadius - 1, s.density,
             BLOCK_LEAVES, seed + 163u);
    leafBlob(b, lx, topY + 1, lz, 1, s.density, BLOCK_LEAVES, seed + 167u);

    // Liana curtains: strands drop from the canopy underside toward the
    // ground/water — some short, some nearly touching down.
    for (int dx = -s.maxRadius; dx <= s.maxRadius; dx++)
    for (int dz = -s.maxRadius; dz <= s.maxRadius; dz++) {
        const int d2 = dx * dx + dz * dz;
        if (d2 < 2 || d2 > s.maxRadius * s.maxRadius + 1) continue;
        if (rnd(lx + dx * 7, lz + dz * 5, seed + 171u) > 0.38f) continue;
        const int len = 3 + (int)(rnd(lx + dx, lz + dz, seed + 179u) *
                                  (float)(s.trunkHeight - 3));
        const int x = lx + dx, z = lz + dz;
        for (int k = 0; k < len; k++) {
            const int y = topY - 2 - k;
            if (y <= surfY || y <= WATER_LEVEL) break;
            if (x < 0 || x >= CHUNK_W || z < 0 || z >= CHUNK_D) break;
            if (b[x][y][z] != BLOCK_AIR) break;
            b[x][y][z] = BLOCK_VINE;
        }
    }
}

inline void placeTree(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                      int lx, int surfY, int lz,
                      const TreeShape& s, unsigned seed) {
    switch (s.species) {
        case SP_OAK:     buildOak(b, lx, surfY, lz, s, BLOCK_LEAVES, seed); break;
        case SP_AUTUMN:  buildOak(b, lx, surfY, lz, s, BLOCK_LEAVES_AUTUMN, seed); break;
        case SP_BIRCH:   buildBirch(b, lx, surfY, lz, s, seed); break;
        case SP_PINE:    buildPine(b, lx, surfY, lz, s, seed); break;
        case SP_CYPRESS: buildCypress(b, lx, surfY, lz, s, seed); break;
        case SP_MANGROVE:buildMangrove(b, lx, surfY, lz, s, seed); break;
        default:         buildDead(b, lx, surfY, lz, s, seed); break;
    }
}

// Deterministic "does a tree go here?" — decorate() uses this, and spawn
// search reuses it to avoid dropping the player inside a canopy.
inline bool wouldPlaceTree(int wx, int wz, int h, Biomes::Biome bi,
                           unsigned seed, const WorldGen::WorldGenerationConfig& cfg) {
    float patch = 0.5f + 0.5f * Noise::fbm(
        (float)wx, (float)wz, seed + 3011u, 2, 0.018f, 2.0f, 0.5f);
    const float cluster = 0.22f + 1.55f * WorldGen::smooth01(0.18f, 0.74f, patch);
    float density = Biomes::info(bi).treeDensity * cfg.treeDensity * cluster;
    if (bi == Biomes::BIOME_FOREST) density *= 1.60f;
    if (bi == Biomes::BIOME_PINNACLES) density *= 0.70f;
    if (bi == Biomes::BIOME_MOUNTAINS && h > 95) density *= 0.35f;
    return rnd(wx, wz, seed + 4001u) < density;
}

inline float localSlope(int wx, int wz, unsigned seed) {
    const int h0 = WorldGen::terrainHeight((float)wx, (float)wz, seed);
    const int hx = WorldGen::terrainHeight((float)(wx + 3), (float)wz, seed);
    const int hz = WorldGen::terrainHeight((float)wx, (float)(wz + 3), seed);
    const int hxm = WorldGen::terrainHeight((float)(wx - 3), (float)wz, seed);
    const int hzm = WorldGen::terrainHeight((float)wx, (float)(wz - 3), seed);
    const int dxDiff = std::abs(hx - hxm);
    const int dzDiff = std::abs(hz - hzm);
    const int maxDiff = dxDiff > dzDiff ? dxDiff : dzDiff;
    (void)h0;
    return (float)maxDiff;
}

inline void decorate(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                     int cx, int cz,
                     const int height[CHUNK_W][CHUNK_D],
                     const Biomes::Biome biome[CHUNK_W][CHUNK_D],
                     unsigned seed) {
    const WorldGen::WorldGenerationConfig cfg = WorldGen::config(seed);

    for (int lx = 0; lx < CHUNK_W; lx++) {
        for (int lz = 0; lz < CHUNK_D; lz++) {
            const int h = height[lx][lz];
            const Biomes::Biome bi = biome[lx][lz];
            const int wx = cx * CHUNK_W + lx;
            const int wz = cz * CHUNK_D + lz;
            const bool ownerCell = lx >= 3 && lx <= CHUNK_W - 4 &&
                                   lz >= 3 && lz <= CHUNK_D - 4;

            // Mangrove groves colonise the shallows of inland water (lake
            // and river margins). Height-derived biomes call every waterline
            // cell BEACH/OCEAN, so climate comes from the macro region
            // (biome the cell would have above the beach band), and the
            // island interior test keeps groves off the open-sea coast.
            const bool shallowBand = h >= WATER_LEVEL - 3 && h <= WATER_LEVEL + 1;
            bool shallows = false;
            if (shallowBand && ownerCell &&
                (long)wx * wx + (long)wz * wz < 240L * 240L) {
                const Biomes::Biome climate =
                    Biomes::biomeAt((float)wx, (float)wz, WATER_LEVEL + 8, seed);
                shallows = climate == Biomes::BIOME_FOREST ||
                           climate == Biomes::BIOME_PLAINS;
            }
            if (shallows) {
                const float grove = 0.5f + 0.5f * Noise::fbm(
                    (float)wx, (float)wz, seed + 8111u, 2, 0.030f, 2.0f, 0.5f);
                if (grove > 0.56f && rnd(wx, wz, seed + 8101u) < 0.14f) {
                    const TreeShape shape = mangroveShape(wx, wz, seed);
                    if (clearTreeVolume(b, lx, h, lz, shape)) {
                        placeTree(b, lx, h, lz, shape, hash2(wx, wz, seed));
                        continue;
                    }
                }
            }

            if (h <= WATER_LEVEL) continue;

            const uint8_t surf = b[lx][h][lz];
            const bool suitableSurface =
                surf == Biomes::surfaceBlock(bi, h) &&
                surf != BLOCK_SAND && surf != BLOCK_STONE &&
                localSlope(wx, wz, seed) <= 4.0f;

            if (ownerCell && suitableSurface &&
                wouldPlaceTree(wx, wz, h, bi, seed, cfg)) {
                const TreeShape shape = shapeFor(bi, wx, wz, seed);
                if (clearTreeVolume(b, lx, h, lz, shape)) {
                    placeTree(b, lx, h, lz, shape, hash2(wx, wz, seed));
                    continue;
                }
            }

            // Forest-floor set dressing: a fallen trunk or a half-buried
            // boulder, rare enough to stay special.
            if (ownerCell && suitableSurface &&
                (bi == Biomes::BIOME_FOREST || bi == Biomes::BIOME_PLAINS)) {
                const float rf = rnd(wx, wz, seed + 7013u);
                if (rf < 0.0028f && bi == Biomes::BIOME_FOREST) {
                    const int len = 3 + (int)(rnd(wx, wz, seed + 7101u) * 2.0f);
                    const bool alongX = (hash2(wx, wz, seed + 7207u) & 1) != 0;
                    for (int d = 0; d < len; d++)
                        setIfAir(b, lx + (alongX ? d : 0), h + 1,
                                 lz + (alongX ? 0 : d), BLOCK_WOOD);
                    continue;
                }
                if (rf > 0.9975f) {
                    setIfAir(b, lx, h + 1, lz, BLOCK_STONE);
                    if (rnd(wx, wz, seed + 7301u) < 0.5f)
                        setIfAir(b, lx + 1, h + 1, lz, BLOCK_STONE);
                    continue;
                }
            }

            // Ground cover grows in scattered patches, not as a uniform
            // carpet: outside a patch the ground stays clean.
            const float rp = rnd(wx, wz, seed + 5051u);
            const int py = h + 1;
            uint8_t plant = BLOCK_AIR;
            if (bi == Biomes::BIOME_PLAINS || bi == Biomes::BIOME_FOREST ||
                bi == Biomes::BIOME_PINNACLES) {
                const float meadow = 0.5f + 0.5f * Noise::fbm(
                    (float)wx, (float)wz, seed + 6101u, 2, 0.045f, 2.0f, 0.5f);
                const float grassChance = meadow > 0.60f ? 0.14f : 0.012f;
                // Flowers grow in RARE small clusters (own noise field), one
                // species per cluster, dense inside — and most of the map has
                // no flowers at all.
                const float fpatch = 0.5f + 0.5f * Noise::fbm(
                    (float)wx, (float)wz, seed + 6401u, 2, 0.085f, 2.0f, 0.5f);
                if (rp < grassChance) plant = BLOCK_TALL_GRASS;
                else if (fpatch > 0.80f && rp < grassChance + 0.20f)
                    plant = (hash2(wx >> 4, wz >> 4, seed + 6511u) & 1)
                          ? BLOCK_FLOWER_RED : BLOCK_FLOWER_YELLOW;
                else if (bi == Biomes::BIOME_FOREST && meadow < 0.40f &&
                         rp < grassChance + 0.015f)
                    plant = rnd(wx, wz, seed + 6301u) < 0.5f
                          ? BLOCK_MUSHROOM_RED : BLOCK_MUSHROOM_BROWN;
                else if (rp < grassChance + 0.012f && meadow > 0.62f)
                    plant = BLOCK_BUSH;
                else if (rp > 0.9975f)
                    plant = BLOCK_ROCK;               // stray small stones
            } else if (bi == Biomes::BIOME_SNOWY) {
                if (rp < 0.015f) plant = BLOCK_BUSH;
            } else if (bi == Biomes::BIOME_BEACH) {
                // Dune grass in sparse tufts just above the tide line.
                if (rp < 0.020f && h >= WATER_LEVEL + 2) plant = BLOCK_BUSH;
                else if (rp > 0.9965f) plant = BLOCK_ROCK;
            } else if (bi == Biomes::BIOME_DESERT) {
                // Desert silhouettes stay clean; larger rock forms come from terrain.
            } else if (bi == Biomes::BIOME_MOUNTAINS) {
                if (rp < 0.018f) plant = BLOCK_ROCK;
            } else if (bi == Biomes::BIOME_VOLCANIC) {
                if (rp < 0.040f) plant = BLOCK_ROCK;
            }

            if (plant != BLOCK_AIR) setIfAir(b, lx, py, lz, plant);
        }
    }
}

} // namespace Decoration
