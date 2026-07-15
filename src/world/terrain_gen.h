#pragma once
#include "noise.h"
#include "block_types.h"
#include "biome.h"
#include "ore_gen.h"
#include "cave_gen.h"
#include "decoration.h"
#include "structure_gen.h"
#include "chunk.h"
#include <cstdint>

namespace TerrainGen {

// Below this world Y the stone layer turns to deepslate (harder deep rock).
static constexpr int DEEPSLATE_LEVEL = 12;

// Per-chunk generation stats for the debug overlay.
struct GenStats {
    int  maxHeight = 0;                  // tallest terrain column in the chunk
    long caveRemoved = 0;                // solid blocks carved out by caves
    long oreCounts[Ores::ORE_COUNT] = {0}; // ore blocks surviving the carve
    int  structures = 0;                 // structure origins in this chunk
    uint8_t structureType = 0;           // last structure type placed here
};

// Surface (top solid block) height for a world column. Mirrors exactly the
// formula used in generate(), so spawn-finding and gameplay code can query a
// column's ground level without building a whole chunk.
inline int surfaceHeight(float wx, float wz, unsigned seed) {
    float n = Noise::fbm(wx, wz, seed, 6, 0.004f, 2.0f, 0.5f);
    int h = 72 + (int)(n * 24.0f);
    Biomes::Biome b = Biomes::biomeAt(wx, wz, h, seed);
    if (b == Biomes::BIOME_MOUNTAINS) {
        float m = Noise::fbm(wx, wz, seed + 3u, 4, 0.006f);
        h += (int)((m * 0.5f + 0.5f) * 40.0f);
    }
    if (h < 2)   h = 2;
    if (h > 124) h = 124;
    // Ground surface is at least sea level so spawn is never underwater.
    return (h < WATER_LEVEL) ? WATER_LEVEL : h;
}

inline void generate(uint8_t blocks[CHUNK_W][CHUNK_H][CHUNK_D],
                     int cx, int cz, unsigned seed,
                     uint8_t*  outDominantBiome = nullptr,
                     long*     outBlockCount    = nullptr,
                     GenStats* outStats         = nullptr) {
    int           height[CHUNK_W][CHUNK_D];
    Biomes::Biome biome [CHUNK_W][CHUNK_D];
    int           biomeTally[Biomes::BIOME_COUNT] = {0};
    int           maxH = 0;

    // ── 1. Solid terrain with layers + ores ─────────────────────────────────
    for (int lx = 0; lx < CHUNK_W; lx++) {
        for (int lz = 0; lz < CHUNK_D; lz++) {
            int wx = cx * CHUNK_W + lx;
            int wz = cz * CHUNK_D + lz;

            // Base (72) sits ABOVE sea level (WATER_LEVEL=62) so land dominates;
            // only the low valleys dip under water and flood into oceans/lakes.
            float n = Noise::fbm((float)wx, (float)wz, seed, 6, 0.004f, 2.0f, 0.5f);
            int h = 72 + (int)(n * 24.0f);

            Biomes::Biome b = Biomes::biomeAt((float)wx, (float)wz, h, seed);
            if (b == Biomes::BIOME_MOUNTAINS) {
                float m = Noise::fbm((float)wx, (float)wz, seed + 3u, 4, 0.006f);
                h += (int)((m * 0.5f + 0.5f) * 40.0f);
            }
            if (h < 2)   h = 2;
            if (h > 124) h = 124;

            height[lx][lz] = h;
            biome [lx][lz] = b;
            biomeTally[b]++;
            if (h > maxH) maxH = h;

            uint8_t surf = Biomes::surfaceBlock(b, h);
            uint8_t sub  = Biomes::subBlock(b);

            for (int y = 0; y < CHUNK_H; y++) {
                uint8_t bt = BLOCK_AIR;
                if (y == 0) {
                    bt = BLOCK_BEDROCK;                       // solid floor
                } else if (y <= 3) {
                    // Rough bedrock: denser near the bottom, thinning upward.
                    float bh = Noise::value2((float)wx * 3.1f, (float)wz * 3.1f,
                                             seed + 777u + (unsigned)y);
                    bt = (bh > (float)(y - 1) * 0.5f - 0.2f) ? BLOCK_BEDROCK
                                                            : BLOCK_STONE;
                } else if (y < h - 4) {
                    bt = (y < DEEPSLATE_LEVEL) ? BLOCK_DEEPSLATE : BLOCK_STONE;
                    uint8_t ore = Ores::at(wx, y, wz, seed);  // 0 = keep stone
                    if (ore) bt = ore;
                } else if (y < h) {
                    bt = sub;
                } else if (y == h) {
                    bt = surf;
                }
                blocks[lx][y][lz] = bt;
            }

            // Flood open air up to sea level with water.
            for (int y = h + 1; y <= WATER_LEVEL && y < CHUNK_H; y++)
                if (blocks[lx][y][lz] == BLOCK_AIR)
                    blocks[lx][y][lz] = BLOCK_WATER;
        }
    }

    // ── 2. Cave carving ─────────────────────────────────────────────────────
    // Keep a 3-block cap under the surface so caves stay underground (no holes
    // in the terrain and no floating vegetation), and never touch bedrock.
    long carved = 0;
    for (int lx = 0; lx < CHUNK_W; lx++) {
        for (int lz = 0; lz < CHUNK_D; lz++) {
            int wx = cx * CHUNK_W + lx;
            int wz = cz * CHUNK_D + lz;
            int top = height[lx][lz] - 3;
            bool ravCol = Caves::ravineColumn(wx, wz, seed);  // once per column
            for (int y = 1; y < top; y++) {
                uint8_t& cell = blocks[lx][y][lz];
                if (cell == BLOCK_AIR || cell == BLOCK_BEDROCK) continue;
                if ((ravCol && Caves::ravineY(y)) || Caves::carve(wx, y, wz, seed)) {
                    cell = BLOCK_AIR;
                    carved++;
                }
            }
        }
    }

    // ── 3. Vegetation (deterministic, seed-driven, one-time) ─────────────────
    Decoration::decorate(blocks, cx, cz, height, biome, seed);

    // ── 3b. Structures (baked into blocks; cross-chunk deterministic) ────────
    uint8_t structType = 0;
    int structCount = Structures::generate(blocks, cx, cz, seed, &structType);

    // ── 4. Stats ─────────────────────────────────────────────────────────────
    if (outDominantBiome) {
        int best = 0;
        for (int i = 1; i < Biomes::BIOME_COUNT; i++)
            if (biomeTally[i] > biomeTally[best]) best = i;
        *outDominantBiome = (uint8_t)best;
    }

    long nonAir = 0;
    long oreTally[Ores::ORE_COUNT] = {0};
    for (int x = 0; x < CHUNK_W; x++)
    for (int y = 0; y < CHUNK_H; y++)
    for (int z = 0; z < CHUNK_D; z++) {
        uint8_t bt = blocks[x][y][z];
        if (bt != BLOCK_AIR) nonAir++;
        int oi = Ores::indexOf(bt);
        if (oi >= 0) oreTally[oi]++;
    }
    if (outBlockCount) *outBlockCount = nonAir;
    if (outStats) {
        outStats->maxHeight  = maxH;
        outStats->caveRemoved = carved;
        for (int i = 0; i < Ores::ORE_COUNT; i++) outStats->oreCounts[i] = oreTally[i];
        outStats->structures    = structCount;
        outStats->structureType = structType;
    }
}

} // namespace TerrainGen
