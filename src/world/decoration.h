#pragma once
#include "chunk.h"
#include "block_types.h"
#include "biome.h"
#include <cstdint>

// Procedural vegetation. Runs ONCE when a chunk is generated and writes
// straight into the chunk's block array — no per-frame cost, no rebuilds.
// All randomness is derived from (worldX, worldZ, seed) so results are
// deterministic and vary automatically with the world seed.
namespace Decoration {

inline uint32_t hash2(int x, int z, unsigned s) {
    uint32_t n = (uint32_t)(x * 374761393u ^ z * 668265263u ^ s * 2246822519u);
    n ^= n >> 13; n *= 1274126177u; n ^= n >> 16;
    return n;
}
// Deterministic pseudo-random in [0,1)
inline float rnd(int x, int z, unsigned s) {
    return (hash2(x, z, s) & 0xFFFFFF) / (float)0x1000000;
}

inline void setIfAir(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                     int x, int y, int z, uint8_t type) {
    if (x < 0 || x >= CHUNK_W || y < 0 || y >= CHUNK_H || z < 0 || z >= CHUNK_D) return;
    if (b[x][y][z] == BLOCK_AIR) b[x][y][z] = type;
}

// ── Oak: straight trunk + roughly spherical canopy ──────────────────────────
inline void oakTree(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                    int lx, int surfY, int lz, int trunkH) {
    int topY = surfY + trunkH;
    for (int y = surfY + 1; y <= topY && y < CHUNK_H; y++)
        b[lx][y][lz] = BLOCK_WOOD;

    for (int dx = -2; dx <= 2; dx++)
    for (int dy = -1; dy <= 2; dy++)
    for (int dz = -2; dz <= 2; dz++) {
        if (dx*dx + dy*dy + dz*dz > 5) continue;
        setIfAir(b, lx + dx, topY + dy, lz + dz, BLOCK_LEAVES);
    }
}

// ── Spruce: taller trunk + conical layered canopy ───────────────────────────
inline void spruceTree(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                       int lx, int surfY, int lz, int trunkH) {
    int topY = surfY + trunkH;
    for (int y = surfY + 1; y <= topY && y < CHUNK_H; y++)
        b[lx][y][lz] = BLOCK_WOOD;

    int r = 2;
    for (int y = topY - trunkH / 2; y <= topY + 1; y++) {
        for (int dx = -r; dx <= r; dx++)
        for (int dz = -r; dz <= r; dz++) {
            if (dx*dx + dz*dz > r*r) continue;
            setIfAir(b, lx + dx, y, lz + dz, BLOCK_LEAVES);
        }
        if (r > 0 && (topY + 1 - y) % 2 == 0) r--;   // taper upward
    }
    setIfAir(b, lx, topY + 2, lz, BLOCK_LEAVES);       // pointed tip
}

// ── Main entry: decorate a fully-filled chunk ───────────────────────────────
inline void decorate(uint8_t b[CHUNK_W][CHUNK_H][CHUNK_D],
                     int cx, int cz,
                     const int height[CHUNK_W][CHUNK_D],
                     const Biomes::Biome biome[CHUNK_W][CHUNK_D],
                     unsigned seed) {
    for (int lx = 0; lx < CHUNK_W; lx++) {
        for (int lz = 0; lz < CHUNK_D; lz++) {
            int h = height[lx][lz];
            if (h <= WATER_LEVEL) continue;            // underwater / shoreline
            Biomes::Biome bi = biome[lx][lz];

            int wx = cx * CHUNK_W + lx;
            int wz = cz * CHUNK_D + lz;
            int surfY = h;                             // top solid block
            uint8_t surf = b[lx][surfY][lz];

            // ── Trees (kept 2 blocks from chunk edge so canopy fits) ────────
            bool canTree = (lx >= 2 && lx <= CHUNK_W - 3 &&
                            lz >= 2 && lz <= CHUNK_D - 3);
            float treeDensity = Biomes::info(bi).treeDensity;  // biome-driven

            if (canTree && surf == Biomes::surfaceBlock(bi, h) &&
                surf != BLOCK_SAND && rnd(wx, wz, seed) < treeDensity) {
                int trunkH = 4 + (int)(rnd(wx, wz, seed + 1u) * 3.0f);
                if (bi == Biomes::BIOME_SNOWY)
                    spruceTree(b, lx, surfY, lz, trunkH + 2);
                else
                    oakTree(b, lx, surfY, lz, trunkH);
                continue;                              // no ground plant on trunk
            }

            // ── Ground vegetation (single cross-quad block above surface) ───
            float rp = rnd(wx, wz, seed + 50u);
            int   py = surfY + 1;
            uint8_t plant = BLOCK_AIR;

            if (bi == Biomes::BIOME_PLAINS || bi == Biomes::BIOME_FOREST) {
                if      (rp < 0.14f) plant = BLOCK_TALL_GRASS;
                else if (rp < 0.16f) plant = BLOCK_FLOWER_RED;
                else if (rp < 0.18f) plant = BLOCK_FLOWER_YELLOW;
                else if (rp < 0.188f && bi == Biomes::BIOME_FOREST) plant = BLOCK_MUSHROOM_RED;
                else if (rp < 0.196f && bi == Biomes::BIOME_FOREST) plant = BLOCK_MUSHROOM_BROWN;
                else if (rp < 0.205f) plant = BLOCK_BUSH;
            } else if (bi == Biomes::BIOME_SNOWY) {
                if      (rp < 0.03f) plant = BLOCK_BUSH;
                else if (rp < 0.045f) plant = BLOCK_ROCK;
            } else if (bi == Biomes::BIOME_DESERT || bi == Biomes::BIOME_MOUNTAINS) {
                if (rp < 0.02f) plant = BLOCK_ROCK;   // small scattered rocks
            }

            if (plant != BLOCK_AIR)
                setIfAir(b, lx, py, lz, plant);
        }
    }
}

} // namespace Decoration
