#pragma once
#include "block_types.h"
#include "worldgen_common.h"
#include "biome.h"
#include <cstdint>

// ── StarterVillage ────────────────────────────────────────────────────────────
// Builds one unique village near (0,0) using real chunk blocks.
// All geometry is stamped via put(wx,wy,wz,block) — no separate objects.
// The caller collects those calls into WorldSave::addOverride so they persist
// across chunk load/unload cycles.
namespace StarterVillage {

static constexpr int VILLAGE_RADIUS = 21; // half-size of central 42×42 area
static constexpr int CHUNK_H_MAX    = 128;

// Position hash for house variant selection.
static inline uint32_t svHash(int x, int z) {
    uint32_t h = (uint32_t)(x * 374761393) ^ (uint32_t)(z * 668265263);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

// Build the complete village. Returns the number of non-air blocks stamped.
// put(wx, wy, wz, block) must stamp one block at world-space coordinates.
template<class F>
int build(int ox, int oz, unsigned seed, F&& put) {
    int nonAirCount = 0;

    // Thin wrapper: clamp to valid Y range, track non-air count.
    auto p = [&](int wx, int wy, int wz, uint8_t b) {
        if (wy < 1 || wy >= CHUNK_H_MAX) return;
        put(wx, wy, wz, b);
        if (b != BLOCK_AIR) nonAirCount++;
    };

    auto surf = [&](int wx, int wz) -> int {
        return WorldGen::terrainHeight(wx, wz, seed);
    };

    // ── 1. Central plaza (10×10 cobblestone floor) ───────────────────────────
    const int plazaX = ox + 16, plazaZ = oz + 16;
    for (int dx = 0; dx < 10; dx++)
    for (int dz = 0; dz < 10; dz++) {
        int wx = plazaX + dx, wz = plazaZ + dz;
        int sy = surf(wx, wz);
        p(wx, sy+1, wz, BLOCK_AIR);               // clear vegetation
        p(wx, sy,   wz, BLOCK_COBBLESTONE);        // plaza floor
        p(wx, sy-1, wz, BLOCK_COBBLESTONE);        // foundation layer
    }

    // ── 2. Well at plaza centre ──────────────────────────────────────────────
    // 5×5 cobblestone rim, 3×3 water at base, wood posts, plank roof.
    const int wX = ox + 19, wZ = oz + 19;
    int wY = surf(wX + 2, wZ + 2);
    for (int dx = 0; dx < 5; dx++)
    for (int dz = 0; dz < 5; dz++) {
        bool edge = (dx == 0 || dx == 4 || dz == 0 || dz == 4);
        p(wX+dx, wY,   wZ+dz, edge ? BLOCK_COBBLESTONE : BLOCK_WATER);
        p(wX+dx, wY+1, wZ+dz, edge ? BLOCK_COBBLESTONE : BLOCK_AIR);
        p(wX+dx, wY+2, wZ+dz, BLOCK_AIR);
        p(wX+dx, wY+3, wZ+dz, BLOCK_AIR);
    }
    // Four corner posts
    const int cpx[4] = {wX,   wX+4, wX,   wX+4};
    const int cpz[4] = {wZ,   wZ,   wZ+4, wZ+4};
    for (int i = 0; i < 4; i++) {
        p(cpx[i], wY+2, cpz[i], BLOCK_WOOD);
        p(cpx[i], wY+3, cpz[i], BLOCK_WOOD);
    }
    // Plank roof
    for (int dx = -1; dx <= 5; dx++)
    for (int dz = -1; dz <= 5; dz++)
        p(wX+dx, wY+4, wZ+dz, BLOCK_PLANKS);
    // Torch lamp-posts on well roof
    p(wX+2, wY+5, wZ-1, BLOCK_TORCH);
    p(wX+2, wY+5, wZ+5, BLOCK_TORCH);

    // ── 3. Houses ────────────────────────────────────────────────────────────
    // door: 0 = north face (dz==0), 1 = south face (dz==D-1)
    auto house = [&](int hx, int hz, int doorFace) {
        uint32_t v = svHash(hx, hz) % 3;
        const int W  = (v == 1) ? 6 : 7;
        const int D  = 5, H = 4;
        uint8_t wall = (v == 1) ? BLOCK_COBBLESTONE : BLOCK_PLANKS;
        uint8_t post = (v == 0) ? BLOCK_WOOD : wall;
        uint8_t roof = (v == 2) ? BLOCK_COBBLESTONE : BLOCK_WOOD;
        int hy = surf(hx + W/2, hz + D/2);
        if (hy <= WATER_LEVEL + 1) return;

        // Foundation + clear interior
        for (int dx = 0; dx < W; dx++)
        for (int dz = 0; dz < D; dz++) {
            int sy = surf(hx+dx, hz+dz);
            p(hx+dx, sy,   hz+dz, BLOCK_COBBLESTONE);
            p(hx+dx, sy-1, hz+dz, BLOCK_COBBLESTONE);
            p(hx+dx, sy+1, hz+dz, BLOCK_AIR);   // clear vegetation
        }
        // Clear interior volume
        for (int dx = 1; dx < W-1; dx++)
        for (int dz = 1; dz < D-1; dz++)
        for (int dy = 1; dy <= H; dy++)
            p(hx+dx, hy+dy, hz+dz, BLOCK_AIR);

        // Walls
        for (int dx = 0; dx < W; dx++)
        for (int dz = 0; dz < D; dz++) {
            bool edge = (dx==0||dx==W-1||dz==0||dz==D-1);
            if (!edge) continue;
            bool corner = (dx==0||dx==W-1) && (dz==0||dz==D-1);
            for (int dy = 1; dy < H; dy++) {
                uint8_t b = corner ? post : wall;
                if (dx == W/2 && dz == doorFace*(D-1) && dy <= 2) continue; // door gap
                if (dy == 2) {
                    if ((dz==0||dz==D-1) && (dx==1||dx==W-2)) b = BLOCK_GLASS;
                    if ((dx==0||dx==W-1) && (dz==1||dz==D-2)) b = BLOCK_GLASS;
                }
                p(hx+dx, hy+dy, hz+dz, b);
            }
        }

        // Roof
        for (int dx = -1; dx <= W; dx++)
        for (int dz = -1; dz <= D; dz++)
            p(hx+dx, hy+H, hz+dz, roof);

        // Interior furnishings
        p(hx + W/2, hy+2, hz + D/2, BLOCK_TORCH);
        p(hx + 1,   hy+1, hz + 1,   BLOCK_CHEST);
    };

    // North row (doors face south = doorFace 1)
    house(ox +  2, oz + 2, 1);
    house(ox + 16, oz + 2, 1);
    house(ox + 30, oz + 2, 1);
    // South row (doors face north = doorFace 0)
    house(ox +  2, oz + 32, 0);
    house(ox + 16, oz + 32, 0);
    house(ox + 30, oz + 32, 0);

    // ── 4. Farm plots ────────────────────────────────────────────────────────
    // 9×8 farmland blocks with a central water channel.
    auto farm = [&](int fx, int fz) {
        const int FW = 9, FD = 8;
        int fy = surf(fx + FW/2, fz + FD/2);
        if (fy <= WATER_LEVEL + 1) return;
        for (int dx = 0; dx < FW; dx++)
        for (int dz = 0; dz < FD; dz++) {
            int sy = surf(fx+dx, fz+dz);
            p(fx+dx, sy+1, fz+dz, BLOCK_AIR);  // clear vegetation
            if (dz == FD/2) {
                p(fx+dx, sy,   fz+dz, BLOCK_WATER);  // hydration channel
                p(fx+dx, sy-1, fz+dz, BLOCK_STONE);
            } else {
                p(fx+dx, sy,   fz+dz, BLOCK_FARMLAND);
                p(fx+dx, sy-1, fz+dz, BLOCK_DIRT);
            }
        }
        // Corner torches to mark the farm boundary
        p(fx,      fy+1, fz,      BLOCK_TORCH);
        p(fx+FW-1, fy+1, fz,      BLOCK_TORCH);
        p(fx,      fy+1, fz+FD-1, BLOCK_TORCH);
        p(fx+FW-1, fy+1, fz+FD-1, BLOCK_TORCH);
    };

    farm(ox +  2, oz + 14);   // west farm
    farm(ox + 29, oz + 14);   // east farm

    // ── 5. Gravel paths (no torches on streets) ──────────────────────────────
    auto path = [&](int x0, int z0, int x1, int z1) {
        int steps = (abs(x1-x0) > abs(z1-z0)) ? abs(x1-x0) : abs(z1-z0);
        if (steps < 1) return;
        for (int s = 0; s <= steps; s++) {
            int px = x0 + (x1-x0)*s/steps;
            int pz = z0 + (z1-z0)*s/steps;
            int py = surf(px, pz);
            if (py <= WATER_LEVEL) continue;
            p(px,   py+1, pz,   BLOCK_AIR);     // clear above path
            p(px,   py,   pz,   BLOCK_GRAVEL);
            p(px+1, py+1, pz,   BLOCK_AIR);
            p(px+1, py,   pz,   BLOCK_GRAVEL);  // 2-wide path
        }
    };

    const int cx = ox + 21, cz = oz + 21;  // plaza center

    // From north houses (south door at dz==D-1=5, so hz+5=7) to plaza
    path(ox +  5, oz +  7, cx, plazaZ);      // NW house → plaza
    path(ox + 19, oz +  7, cx, plazaZ);      // NC house → plaza
    path(ox + 33, oz +  7, cx, plazaZ);      // NE house → plaza
    // From south houses (north door at dz==0, so hz=32) to plaza
    path(ox +  5, oz + 32, cx, plazaZ + 10); // SW house → plaza
    path(ox + 19, oz + 32, cx, plazaZ + 10); // SC house → plaza
    path(ox + 33, oz + 32, cx, plazaZ + 10); // SE house → plaza
    // From farms to plaza sides
    path(ox + 11, oz + 18, plazaX,      cz); // west farm → plaza
    path(ox + 29, oz + 18, plazaX + 10, cz); // east farm → plaza

    // ── 6. Plaza (no torches — lit by house windows) ─────────────────────────

    return nonAirCount;
}

// Scan outward from (0,0) for a flat plains area (slope ≤ 5 over 38 blocks).
// Returns true and fills ox, oz (top-left of village 40×40 area).
inline bool findSpawn(unsigned seed, int& ox, int& oz) {
    const int STEP = 16, MAX_R = 1200;
    for (int r = 0; r <= MAX_R; r += STEP) {
        for (int dz = -r; dz <= r; dz += STEP) {
            for (int dx = -r; dx <= r; dx += STEP) {
                if (r > 0 && abs(dx) != r && abs(dz) != r) continue;
                // Check biome at candidate centre
                int centX = dx + 20, centZ = dz + 20;
                int hc = WorldGen::terrainHeight(centX, centZ, seed);
                if (Biomes::biomeAt((float)centX, (float)centZ, hc, seed)
                    != Biomes::BIOME_PLAINS) continue;
                if (hc <= WATER_LEVEL + 3) continue;
                // All four corners of 38×38 area must be reasonably flat
                int hcorner[4] = {
                    WorldGen::terrainHeight(dx,    dz,    seed),
                    WorldGen::terrainHeight(dx+38, dz,    seed),
                    WorldGen::terrainHeight(dx,    dz+38, seed),
                    WorldGen::terrainHeight(dx+38, dz+38, seed),
                };
                int hmin = hcorner[0], hmax = hcorner[0];
                for (int i = 1; i < 4; i++) {
                    if (hcorner[i] < hmin) hmin = hcorner[i];
                    if (hcorner[i] > hmax) hmax = hcorner[i];
                }
                if (hmin <= WATER_LEVEL + 3) continue;
                if (hmax - hmin > 5) continue;     // village self-adapts, but limit slope
                ox = dx;
                oz = dz;
                return true;
            }
        }
    }
    return false;
}

} // namespace StarterVillage
