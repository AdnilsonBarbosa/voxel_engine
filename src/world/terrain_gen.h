#pragma once
#include "world_generation_config.h"
#include "block_types.h"
#include "ore_gen.h"
#include "cave_gen.h"
#include "decoration.h"
#include "structure_gen.h"
#include "chunk.h"
#include <cstdint>

namespace TerrainGen {

static constexpr int DEEPSLATE_LEVEL = 12;

struct GenStats {
    int maxHeight = 0;
    long caveRemoved = 0;
    long oreCounts[Ores::ORE_COUNT] = {0};
    int structures = 0;
    uint8_t structureType = 0;
};

inline int columnHeight(float wx, float wz, unsigned seed) {
    return WorldGen::terrainHeight(wx, wz, seed);
}

inline int surfaceHeight(float wx, float wz, unsigned seed) {
    const int h = columnHeight(wx, wz, seed);
    return h < WATER_LEVEL ? WATER_LEVEL : h;
}

inline void generate(uint8_t blocks[CHUNK_W][CHUNK_H][CHUNK_D],
                     int cx, int cz, unsigned seed,
                     uint8_t* outDominantBiome = nullptr,
                     long* outBlockCount = nullptr,
                     GenStats* outStats = nullptr) {
    int height[CHUNK_W][CHUNK_D];
    Biomes::Biome biome[CHUNK_W][CHUNK_D];
    int biomeTally[Biomes::BIOME_COUNT] = {0};
    int maxH = 0;

    for (int lx = 0; lx < CHUNK_W; lx++) {
        for (int lz = 0; lz < CHUNK_D; lz++) {
            const int wx = cx * CHUNK_W + lx;
            const int wz = cz * CHUNK_D + lz;
            const int h = columnHeight((float)wx, (float)wz, seed);
            const Biomes::Biome b = Biomes::biomeAt((float)wx, (float)wz, h, seed);

            height[lx][lz] = h;
            biome[lx][lz] = b;
            biomeTally[(int)b]++;
            if (h > maxH) maxH = h;

            const uint8_t surf = Biomes::surfaceBlock(b, h);
            const uint8_t sub = Biomes::subBlock(b);

            for (int y = 0; y < CHUNK_H; y++) {
                uint8_t block = BLOCK_AIR;
                if (y == 0) {
                    block = BLOCK_BEDROCK;
                } else if (y <= 3) {
                    const float bedNoise = Noise::value2(
                        (float)wx * 3.1f, (float)wz * 3.1f,
                        seed + 777u + (unsigned)y);
                    block = bedNoise > (float)(y - 1) * 0.5f - 0.2f
                          ? BLOCK_BEDROCK : BLOCK_STONE;
                } else if (y < h - 4) {
                    block = y < DEEPSLATE_LEVEL ? BLOCK_DEEPSLATE : BLOCK_STONE;
                    const uint8_t ore = Ores::at(wx, y, wz, seed);
                    if (ore) block = ore;
                } else if (y < h) {
                    block = sub;
                } else if (y == h) {
                    block = surf;
                }
                blocks[lx][y][lz] = block;
            }

            // Water is only filled from a real column top to sea level. Since
            // all columns use the same height function, rivers stay contained
            // across chunk borders instead of forming floating sheets.
            for (int y = h + 1; y <= WATER_LEVEL && y < CHUNK_H; y++)
                if (blocks[lx][y][lz] == BLOCK_AIR)
                    blocks[lx][y][lz] = BLOCK_WATER;
        }
    }

    long carved = 0;
    for (int lx = 0; lx < CHUNK_W; lx++) {
        for (int lz = 0; lz < CHUNK_D; lz++) {
            const int wx = cx * CHUNK_W + lx;
            const int wz = cz * CHUNK_D + lz;
            // Six solid blocks remain below the surface. This prevents the
            // local cave test from opening a hole where a neighbouring column
            // is lower, which was the source of many exposed cave ceilings.
            const int top = height[lx][lz] - 6;
            const Biomes::Biome columnBiome = biome[lx][lz];
            const bool solidMass = columnBiome == Biomes::BIOME_PINNACLES ||
                                   columnBiome == Biomes::BIOME_VOLCANIC ||
                                   (columnBiome == Biomes::BIOME_MOUNTAINS && height[lx][lz] > 92);
            if (top <= 8 || solidMass) continue;
            const bool ravine = Caves::ravineColumn(wx, wz, seed);
            for (int y = 6; y < top; y++) {
                uint8_t& cell = blocks[lx][y][lz];
                if (cell == BLOCK_AIR || cell == BLOCK_BEDROCK) continue;
                if ((ravine && Caves::ravineY(y)) ||
                    Caves::carve(wx, y, wz, seed)) {
                    cell = BLOCK_AIR;
                    carved++;
                }
            }
        }
    }

    // ── Lava chambers: deep carved caves flood with lava ─────────────────────
    // Underground only (never surfaces: the flood is capped well below the
    // water table). Basalt crusts form around the melt, hiding rare ores.
    static constexpr int LAVA_LEVEL = 13;
    for (int lx = 0; lx < CHUNK_W; lx++)
    for (int lz = 0; lz < CHUNK_D; lz++) {
        const int top = height[lx][lz] - 6;
        for (int y = 6; y < top && y <= LAVA_LEVEL; y++)
            if (blocks[lx][y][lz] == BLOCK_AIR)
                blocks[lx][y][lz] = BLOCK_LAVA;
    }
    for (int lx = 0; lx < CHUNK_W; lx++)
    for (int lz = 0; lz < CHUNK_D; lz++)
    for (int y = 6; y <= LAVA_LEVEL + 1 && y < CHUNK_H; y++) {
        if (blocks[lx][y][lz] != BLOCK_LAVA) continue;
        static const int N[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        for (int n = 0; n < 6; n++) {
            const int nx = lx + N[n][0], ny = y + N[n][1], nz = lz + N[n][2];
            if (nx < 0 || nx >= CHUNK_W || ny < 1 || ny >= CHUNK_H ||
                nz < 0 || nz >= CHUNK_D) continue;
            uint8_t& nb = blocks[nx][ny][nz];
            if (nb != BLOCK_STONE && nb != BLOCK_DEEPSLATE) continue;
            const uint32_t h = (uint32_t)((nx + cx*CHUNK_W) * 374761393
                             ^ ny * 668265263 ^ (nz + cz*CHUNK_D) * 2246822519u
                             ^ seed);
            const uint32_t r = (h ^ (h >> 13)) % 100u;
            if      (r < 62) nb = BLOCK_BASALT;
            else if (r < 65) nb = BLOCK_DIAMOND;
            else if (r < 69) nb = BLOCK_GOLD;
        }
    }

    Decoration::decorate(blocks, cx, cz, height, biome, seed);

    uint8_t structType = 0;
    const int structCount = WorldGen::config(seed).structures ?
        Structures::generate(blocks, cx, cz, seed, &structType) : 0;

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
        const uint8_t block = blocks[x][y][z];
        if (block != BLOCK_AIR) nonAir++;
        const int oreIndex = Ores::indexOf(block);
        if (oreIndex >= 0) oreTally[oreIndex]++;
    }

    if (outBlockCount) *outBlockCount = nonAir;
    if (outStats) {
        outStats->maxHeight = maxH;
        outStats->caveRemoved = carved;
        for (int i = 0; i < Ores::ORE_COUNT; i++)
            outStats->oreCounts[i] = oreTally[i];
        outStats->structures = structCount;
        outStats->structureType = structType;
    }
}

} // namespace TerrainGen
