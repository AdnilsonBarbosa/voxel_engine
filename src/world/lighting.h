#pragma once
// LightingSystem — Minecraft Bedrock-style baked lighting.
// Computes skylight (BFS from open sky) and block light (BFS from emissive blocks)
// with cross-chunk seeding via ChunkNeighbors.  All work is done on the main
// thread inside buildMesh(), so zero per-frame GPU overhead — mobile-friendly.
#include "chunk.h"
#include "block_material.h"
#include <cstring>
#include <vector>

namespace Lighting {

// ── Sky light ──────────────────────────────────────────────────────────────
// Phase 1 — column scan: every transparent cell above the first opaque block
//            in its column gets sky = 15.  Handles roofless open air in O(N).
// Phase 2 — cross-chunk seed: copy adjacent chunks' border sky values inward
//            (one cell deep) so sun-lit areas bleed correctly across borders.
// Phase 3 — BFS: flood skylight sideways and downward into overhangs / caves
//            with –1 attenuation per step, same rule as block light.
inline void computeSky(
    uint8_t       sky[CHUNK_W][CHUNK_H][CHUNK_D],
    const uint8_t blk[CHUNK_W][CHUNK_H][CHUNK_D],
    const ChunkNeighbors& nb)
{
    memset(sky, 0, (size_t)(CHUNK_W * CHUNK_H * CHUNK_D));

    std::vector<int> q;
    q.reserve(CHUNK_W * CHUNK_D * 8); // typical: ~surface cells

    // Phase 1: column scan
    for (int x = 0; x < CHUNK_W; x++)
    for (int z = 0; z < CHUNK_D; z++) {
        for (int y = CHUNK_H - 1; y >= 0; y--) {
            if (blockIsOpaque(blk[x][y][z])) break;
            sky[x][y][z] = 15;
            q.push_back((x * CHUNK_H + y) * CHUNK_D + z);
        }
    }

    // Phase 2: cross-chunk border seeding
    auto seedBorder = [&](const Chunk* src, int sx, int sy, int sz,
                                            int dx, int dy, int dz) {
        if (!src || !src->lightValid) return;
        uint8_t nv = src->lightSky[sx][sy][sz];
        if (nv <= 1) return;
        uint8_t cv = (uint8_t)(nv - 1);
        if (cv > sky[dx][dy][dz] && !blockIsOpaque(blk[dx][dy][dz])) {
            sky[dx][dy][dz] = cv;
            q.push_back((dx * CHUNK_H + dy) * CHUNK_D + dz);
        }
    };
    for (int y = 0; y < CHUNK_H; y++)
    for (int z = 0; z < CHUNK_D; z++) {
        seedBorder(nb.nx, CHUNK_W-1, y, z,       0,        y, z);
        seedBorder(nb.px, 0,         y, z,       CHUNK_W-1, y, z);
    }
    for (int y = 0; y < CHUNK_H; y++)
    for (int x = 0; x < CHUNK_W; x++) {
        seedBorder(nb.nz, x, y, CHUNK_D-1,       x, y, 0);
        seedBorder(nb.pz, x, y, 0,               x, y, CHUNK_D-1);
    }

    // Phase 3: BFS — spread sideways / downward, –1 per hop
    static const int D6[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (int qi = 0; qi < (int)q.size(); qi++) {
        int idx = q[qi];
        int z = idx % CHUNK_D;
        int y = (idx / CHUNK_D) % CHUNK_H;
        int x = idx / (CHUNK_D * CHUNK_H);
        int L = sky[x][y][z];
        if (L <= 1) continue;
        for (int k = 0; k < 6; k++) {
            int nx = x + D6[k][0], ny = y + D6[k][1], nz = z + D6[k][2];
            if (nx<0||nx>=CHUNK_W||ny<0||ny>=CHUNK_H||nz<0||nz>=CHUNK_D) continue;
            if (blockIsOpaque(blk[nx][ny][nz])) continue;
            int nL = L - 1;
            if ((uint8_t)nL > sky[nx][ny][nz]) {
                sky[nx][ny][nz] = (uint8_t)nL;
                q.push_back((nx * CHUNK_H + ny) * CHUNK_D + nz);
            }
        }
    }
}

// ── Block light ────────────────────────────────────────────────────────────
// BFS from emissive blocks in this chunk (torches, lanterns, lava, etc.)
// plus cross-chunk seeding from adjacent chunks' border light values.
inline void computeBlock(
    uint8_t       blight[CHUNK_W][CHUNK_H][CHUNK_D],
    const uint8_t blk[CHUNK_W][CHUNK_H][CHUNK_D],
    const ChunkNeighbors& nb)
{
    memset(blight, 0, (size_t)(CHUNK_W * CHUNK_H * CHUNK_D));

    std::vector<int> q;
    q.reserve(512);

    // Seed from emissive blocks in this chunk
    for (int x = 0; x < CHUNK_W; x++)
    for (int y = 0; y < CHUNK_H; y++)
    for (int z = 0; z < CHUNK_D; z++) {
        uint8_t e = Materials::of(blk[x][y][z]).lightEmit;
        if (e) {
            blight[x][y][z] = e;
            q.push_back((x * CHUNK_H + y) * CHUNK_D + z);
        }
    }

    // Cross-chunk seeding from neighbor borders
    auto seedBorder = [&](const Chunk* src, int sx, int sy, int sz,
                                            int dx, int dy, int dz) {
        if (!src || !src->lightValid) return;
        uint8_t nv = src->lightBlock[sx][sy][sz];
        if (nv <= 1) return;
        uint8_t cv = (uint8_t)(nv - 1);
        if (cv > blight[dx][dy][dz] && !blockIsOpaque(blk[dx][dy][dz])) {
            blight[dx][dy][dz] = cv;
            q.push_back((dx * CHUNK_H + dy) * CHUNK_D + dz);
        }
    };
    for (int y = 0; y < CHUNK_H; y++)
    for (int z = 0; z < CHUNK_D; z++) {
        seedBorder(nb.nx, CHUNK_W-1, y, z,       0,        y, z);
        seedBorder(nb.px, 0,         y, z,       CHUNK_W-1, y, z);
    }
    for (int y = 0; y < CHUNK_H; y++)
    for (int x = 0; x < CHUNK_W; x++) {
        seedBorder(nb.nz, x, y, CHUNK_D-1,       x, y, 0);
        seedBorder(nb.pz, x, y, 0,               x, y, CHUNK_D-1);
    }

    // BFS propagation within this chunk
    static const int D6[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (int qi = 0; qi < (int)q.size(); qi++) {
        int idx = q[qi];
        int z = idx % CHUNK_D;
        int y = (idx / CHUNK_D) % CHUNK_H;
        int x = idx / (CHUNK_D * CHUNK_H);
        int L = blight[x][y][z];
        if (L <= 1) continue;
        for (int k = 0; k < 6; k++) {
            int nx = x + D6[k][0], ny = y + D6[k][1], nz = z + D6[k][2];
            if (nx<0||nx>=CHUNK_W||ny<0||ny>=CHUNK_H||nz<0||nz>=CHUNK_D) continue;
            if (blockIsOpaque(blk[nx][ny][nz])) continue;
            if (blight[nx][ny][nz] < (uint8_t)(L-1)) {
                blight[nx][ny][nz] = (uint8_t)(L-1);
                q.push_back((nx * CHUNK_H + ny) * CHUNK_D + nz);
            }
        }
    }
}

// ── Main entry ─────────────────────────────────────────────────────────────
// Computes both sky and block light and marks the chunk as having valid light.
// Call this at the top of buildMesh() before reading lightSky / lightBlock.
inline void computeAll(Chunk& c, const ChunkNeighbors& nb) {
    computeSky  (c.lightSky,   c.blocks, nb);
    computeBlock(c.lightBlock, c.blocks, nb);
    c.lightValid = true;
}

// ── Debug stats ────────────────────────────────────────────────────────────
// Count solid (non-air) blocks by combined light level for HUD display.
inline void countStats(const Chunk& c, int& litOut, int& darkOut) {
    litOut = darkOut = 0;
    for (int x = 0; x < CHUNK_W; x++)
    for (int y = 0; y < CHUNK_H; y++)
    for (int z = 0; z < CHUNK_D; z++) {
        if (c.blocks[x][y][z] == 0) continue;
        uint8_t L = (c.lightSky[x][y][z] > c.lightBlock[x][y][z])
                    ? c.lightSky[x][y][z] : c.lightBlock[x][y][z];
        if (L >= 8) litOut++; else darkOut++;
    }
}

} // namespace Lighting
