#pragma once
#include "block_types.h"
#include "biome.h"
#include "worldgen_common.h"
#include "chunk.h"
#include <cstdint>

// ── Procedural structures ────────────────────────────────────────────────────
// Structures are baked directly into chunk blocks (no objects). Placement is a
// deterministic grid: the world is divided into CELL×CELL regions, each holding
// at most one candidate structure at a seed-derived position. A structure that
// straddles a chunk border is produced identically by every overlapping chunk
// and each chunk stamps only the part inside it — so cross-chunk builds need no
// communication and stay perfectly deterministic.
//
// Architecture (extensible to hundreds of structures):
//   StructureRegistry  — REGISTRY[] table (name, footprint, rarity, biomes)
//   StructureManager   — Structures::generate() (placement + terrain fit + stamp)
//   *Generator         — build* templates (Cabin/House/Ruins/Tower/Well/…)
namespace Structures {

enum Type : uint8_t {
    S_NONE,
    S_CABIN, S_HOUSE, S_RUINS, S_TOWER, S_WELL, S_CAMP, S_BRIDGE,
    S_VILLAGE, S_TEMPLE, S_DUNGEON, S_MINE, S_CASTLE, S_FORTRESS, S_PORTAL,
    // Natural landmarks (compact-map POIs)
    S_MONOLITH, S_ARCH, S_GREATTREE,
    S_COUNT
};

// Biome bit helpers
static inline uint8_t bmask(Biomes::Biome b) { return (uint8_t)(1u << b); }

struct Def {
    const char* name;
    int         footprint;   // horizontal extent (blocks)
    float       rarity;      // chance a valid cell in-biome spawns it
    uint8_t     biomes;      // allowed biomes (bit per Biomes::Biome)
    bool        implemented; // has a builder yet
};

// StructureRegistry
static const Def REGISTRY[S_COUNT] = {
//   name          foot rarity biomes                                                              impl
    {"None",         0,  0.00f, 0, false},
    {"Cabin",        6,  0.45f, (uint8_t)(bmask(Biomes::BIOME_FOREST)|bmask(Biomes::BIOME_SNOWY)), true},
    {"House",        7,  0.40f, bmask(Biomes::BIOME_PLAINS),    true},
    {"Ruins",        7,  0.45f, bmask(Biomes::BIOME_DESERT),    true},
    {"Watchtower",   4,  0.55f, bmask(Biomes::BIOME_MOUNTAINS), true},
    {"Well",         4,  0.30f, bmask(Biomes::BIOME_PLAINS),    true},
    {"Camp",         5,  0.35f, bmask(Biomes::BIOME_FOREST),    true},
    {"Bridge",       6,  0.25f, bmask(Biomes::BIOME_PLAINS),    true},
    {"Village",     38,  0.55f, bmask(Biomes::BIOME_PLAINS), true},
    {"Temple",      11,  0.40f, bmask(Biomes::BIOME_DESERT), true},
    {"Dungeon",      8,  0.80f, 0xFF, true},   // underground, any land
    {"Abandoned Mine",6, 0.80f, 0xFF, true},   // underground, any land
    {"Castle",      16,  0.85f, (uint8_t)(bmask(Biomes::BIOME_PLAINS)|bmask(Biomes::BIOME_MOUNTAINS)), true},
    {"Fortress",    14,  0.85f, (uint8_t)(bmask(Biomes::BIOME_MOUNTAINS)|bmask(Biomes::BIOME_DESERT)), true},
    {"Portal",       5,  0.80f, 0xFF, true},   // any land
    {"Monolith",     7,  0.55f, 0xFF, true},   // giant standing stones
    {"Stone Arch",  10,  0.50f, (uint8_t)(bmask(Biomes::BIOME_PLAINS)|bmask(Biomes::BIOME_DESERT)|bmask(Biomes::BIOME_MOUNTAINS)), true},
    {"Great Tree",  13,  0.60f, (uint8_t)(bmask(Biomes::BIOME_FOREST)|bmask(Biomes::BIOME_PLAINS)), true},
};

inline const Def& def(Type t)  { return REGISTRY[t]; }
inline int         count()     { return S_COUNT; }
inline bool        isUnderground(Type t) { return t == S_DUNGEON || t == S_MINE; }

static constexpr int CELL = 48;   // one candidate structure per 48×48 region

static inline uint32_t hash2(int x, int z, unsigned seed) {
    uint32_t h = (uint32_t)(x*374761393) ^ (uint32_t)(z*668265263) ^ (seed*2246822519u);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

// Which structure type a biome offers for a given cell hash. Big structures
// (village/temple) appear on a fraction of cells; the rest get small ones.
inline Type candidateFor(Biomes::Biome b, uint32_t h) {
    if (b == Biomes::BIOME_OCEAN || b == Biomes::BIOME_BEACH) return S_NONE;

    // Rare cross-biome specials take priority over the biome's common builds.
    int s = (h >> 5) % 100;
    if (s < 3) return S_DUNGEON;                               // underground
    if (s < 5) return S_MINE;                                  // underground
    if (s < 6) return S_PORTAL;
    if (s < 8 && (b == Biomes::BIOME_PLAINS || b == Biomes::BIOME_MOUNTAINS)) return S_CASTLE;
    if (s < 10 && (b == Biomes::BIOME_MOUNTAINS || b == Biomes::BIOME_DESERT)) return S_FORTRESS;
    if (s < 16) return S_MONOLITH;                             // standing stones
    if (s < 21 && (b == Biomes::BIOME_PLAINS || b == Biomes::BIOME_DESERT ||
                   b == Biomes::BIOME_MOUNTAINS)) return S_ARCH;

    switch (b) {
        case Biomes::BIOME_FOREST:
            return ((h & 7) == 0) ? S_GREATTREE
                 : (h & 1)        ? S_CABIN : S_CAMP;
        case Biomes::BIOME_SNOWY:     return S_CABIN;
        case Biomes::BIOME_MOUNTAINS: return S_TOWER;
        case Biomes::BIOME_PLAINS:
            switch ((h >> 1) % 5) {
                case 0:  return ((h >> 9) & 1) ? S_VILLAGE : S_HOUSE; // sparse hamlets
                case 1:  return S_WELL;
                case 2:  return S_BRIDGE;
                case 3:  return S_GREATTREE;
                default: return S_HOUSE;
            }
        case Biomes::BIOME_DESERT:
            return ((h >> 3) % 4 == 0) ? S_TEMPLE : S_RUINS;   // ~1 in 4 desert cells
        default:                      return S_NONE;
    }
}

// StructureManager: deterministic candidate for grid cell (gx,gz). Placement is
// footprint-aware so a structure always fits inside its own cell → structures
// from neighbouring cells can never overlap, whatever their size.
inline Type cellPick(int gx, int gz, unsigned seed, int& ox, int& oz) {
    uint32_t h = hash2(gx, gz, seed + 9001u);

    // Sample the biome at the cell centre to decide the type.
    Biomes::Biome b = WorldGen::biomeAt(gx * CELL + CELL/2, gz * CELL + CELL/2, seed);
    Type t = candidateFor(b, h);
    if (t == S_NONE || !REGISTRY[t].implemented) return S_NONE;

    float r = ((h >> 16) & 0xFFFF) / 65535.0f;
    if (r > REGISTRY[t].rarity) return S_NONE;

    int avail = CELL - REGISTRY[t].footprint; if (avail < 1) avail = 1;
    ox = gx * CELL + (int)((h & 0xFFFF)        % avail);
    oz = gz * CELL + (int)(((h >> 20) & 0xFFF) % avail);
    return t;
}

// ── Builders (StructureGenerator / *Generator) ───────────────────────────────
// `put(wx,wy,wz,block)` stamps one block (ignored if outside the active chunk).

template<class F> void buildCabin(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int W = 5, D = 5, H = 4;
    for (int dx = 0; dx < W; dx++)
    for (int dz = 0; dz < D; dz++) {
        put(ox+dx, oy, oz+dz, BLOCK_PLANKS);                       // floor
        for (int dy = 1; dy < H; dy++) {
            bool edge = (dx==0||dx==W-1||dz==0||dz==D-1);
            if (!edge) continue;
            bool corner = (dx==0||dx==W-1) && (dz==0||dz==D-1);
            uint8_t b = corner ? BLOCK_WOOD : BLOCK_PLANKS;
            if (dx==2 && dz==0 && dy<3) continue;                  // doorway
            if ((dx==2 && (dz==D-1)) && dy==2) b = BLOCK_GLASS;    // window
            if ((dz==2 && (dx==0||dx==W-1)) && dy==2) b = BLOCK_GLASS;
            put(ox+dx, oy+dy, oz+dz, b);
        }
    }
    for (int dx = -1; dx <= W; dx++)                               // overhanging roof
    for (int dz = -1; dz <= D; dz++)
        put(ox+dx, oy+H, oz+dz, BLOCK_WOOD);
    put(ox+2, oy+1, oz+2, BLOCK_CHEST);                           // loot
}

template<class F> void buildHouse(int ox, int oy, int oz, unsigned seed, F&& put) {
    // Three house styles chosen by position (seed-deterministic variety).
    uint32_t vh = hash2(ox, oz, seed + 77u);
    int variant = vh % 3;
    int  W = (variant == 1) ? 7 : (variant == 2 ? 5 : 6);
    int  D = (variant == 1) ? 6 : 5;
    int  H = 4;
    uint8_t wall  = (variant == 2) ? BLOCK_COBBLESTONE : BLOCK_PLANKS;
    uint8_t post  = (variant == 1) ? BLOCK_WOOD : wall;
    uint8_t roof  = (variant == 0) ? BLOCK_PLANKS : BLOCK_WOOD;

    for (int dx = 0; dx < W; dx++)
    for (int dz = 0; dz < D; dz++) {
        put(ox+dx, oy, oz+dz, BLOCK_COBBLESTONE);                 // stone foundation
        for (int dy = 1; dy < H; dy++) {
            bool edge = (dx==0||dx==W-1||dz==0||dz==D-1);
            if (!edge) continue;
            bool corner = (dx==0||dx==W-1) && (dz==0||dz==D-1);
            uint8_t b = (dy == 1) ? BLOCK_COBBLESTONE : (corner ? post : wall);
            if (dx==W/2 && dz==0 && dy<3) continue;                // door
            if (dy==2 && ((dx==W-2&&dz==0)||(dz==D/2&&(dx==0||dx==W-1)))) b = BLOCK_GLASS;
            put(ox+dx, oy+dy, oz+dz, b);
        }
    }
    for (int dx = -1; dx <= W; dx++)
    for (int dz = -1; dz <= D; dz++)
        put(ox+dx, oy+H, oz+dz, roof);                            // roof
    // Pitched silhouette: a second, inset roof level (variant 2 stays flat —
    // three distinct house shapes per village).
    if (variant != 2)
        for (int dx = 1; dx < W-1; dx++)
        for (int dz = 1; dz < D-1; dz++)
            put(ox+dx, oy+H+1, oz+dz, roof);
    if (variant == 1)                                             // ridge cap
        for (int dx = 2; dx < W-2; dx++)
            put(ox+dx, oy+H+2, oz+D/2, BLOCK_WOOD);
    put(ox+W-2, oy+1, oz+D-2, BLOCK_CHEST);
    put(ox+1,   oy+3, oz+1,   BLOCK_TORCH);                       // interior light
}

template<class F> void buildRuins(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int W = 6, D = 6, H = 4;
    for (int dx = 0; dx < W; dx++)
    for (int dz = 0; dz < D; dz++) {
        bool edge = (dx==0||dx==W-1||dz==0||dz==D-1);
        if (!edge) continue;
        int wallH = 1 + (int)(hash2(ox+dx, oz+dz, seed+13u) % (H));  // crumbled height
        for (int dy = 1; dy <= wallH; dy++) {
            if (hash2(ox+dx, oz+dz*7+dy, seed+31u) % 5 == 0) continue; // missing blocks
            put(ox+dx, oy+dy, oz+dz, BLOCK_SAND);
        }
    }
    for (int dx = 1; dx < W-1; dx++)                               // broken floor
    for (int dz = 1; dz < D-1; dz++)
        if (hash2(ox+dx, oz+dz, seed+51u) % 3) put(ox+dx, oy, oz+dz, BLOCK_SAND);
    put(ox+2, oy+1, oz+2, BLOCK_CHEST);
}

template<class F> void buildTower(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int W = 3, D = 3, H = 9;
    for (int dy = 0; dy < H; dy++)
    for (int dx = 0; dx < W; dx++)
    for (int dz = 0; dz < D; dz++) {
        bool edge = (dx==0||dx==W-1||dz==0||dz==D-1);
        if (dy == 0) { put(ox+dx, oy, oz+dz, BLOCK_STONE); continue; }
        if (!edge) continue;
        if (dx==1 && dz==0 && dy>=1 && dy<=2) continue;           // entrance
        put(ox+dx, oy+dy, oz+dz, BLOCK_COBBLESTONE);
    }
    for (int dx = -1; dx <= W; dx++)                               // battlement ring
    for (int dz = -1; dz <= D; dz++)
        if ((dx+dz) % 2 == 0) put(ox+dx, oy+H, oz+dz, BLOCK_COBBLESTONE);
    put(ox+1, oy+1, oz+1, BLOCK_CHEST);
}

template<class F> void buildWell(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int W = 3, D = 3;
    for (int dx = 0; dx < W; dx++)
    for (int dz = 0; dz < D; dz++) {
        bool edge = (dx==0||dx==W-1||dz==0||dz==D-1);
        if (edge) { put(ox+dx, oy,   oz+dz, BLOCK_COBBLESTONE);
                    put(ox+dx, oy+1, oz+dz, BLOCK_COBBLESTONE); }
        else        put(ox+dx, oy,   oz+dz, BLOCK_WATER);
    }
    put(ox,     oy+2, oz,     BLOCK_WOOD); put(ox+W-1, oy+2, oz,     BLOCK_WOOD);
    put(ox,     oy+2, oz+D-1, BLOCK_WOOD); put(ox+W-1, oy+2, oz+D-1, BLOCK_WOOD);
    for (int dx = -1; dx <= W; dx++)                               // little roof
    for (int dz = -1; dz <= D; dz++)
        put(ox+dx, oy+3, oz+dz, BLOCK_PLANKS);
}

template<class F> void buildCamp(int ox, int oy, int oz, unsigned seed, F&& put) {
    // Ring of logs around a small fire, plus a supply chest.
    put(ox,   oy+1, oz,   BLOCK_WOOD); put(ox+3, oy+1, oz,   BLOCK_WOOD);
    put(ox,   oy+1, oz+3, BLOCK_WOOD); put(ox+3, oy+1, oz+3, BLOCK_WOOD);
    put(ox+1, oy+1, oz+1, BLOCK_WOOD); put(ox+2, oy+1, oz+2, BLOCK_WOOD); // fire logs
    put(ox+2, oy+1, oz,   BLOCK_CHEST);
}

template<class F> void buildBridge(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int L = 6;
    for (int dx = 0; dx < L; dx++) {                              // plank walkway
        put(ox+dx, oy+2, oz,   BLOCK_PLANKS);
        put(ox+dx, oy+2, oz+1, BLOCK_PLANKS);
        put(ox+dx, oy+3, oz-1, (dx%2)?BLOCK_AIR:BLOCK_WOOD);      // railing posts
        put(ox+dx, oy+3, oz+2, (dx%2)?BLOCK_AIR:BLOCK_WOOD);
    }
    put(ox,     oy+1, oz,   BLOCK_WOOD); put(ox+L-1, oy+1, oz,   BLOCK_WOOD);  // supports
    put(ox,     oy+1, oz+1, BLOCK_WOOD); put(ox+L-1, oy+1, oz+1, BLOCK_WOOD);
}

// ── VillageGenerator ─────────────────────────────────────────────────────────
// A cluster of small buildings + a central well, each placed on its OWN ground
// height so the whole village follows the terrain instead of needing a flat
// 38×38 patch. A per-village hash varies which lots are built.
template<class F> void buildVillage(int ox, int oz, unsigned seed, F&& put) {
    struct Lot { int dx, dz; Type type; };
    static const Lot LOTS[] = {
        { 1,  1, S_HOUSE}, { 15,  1, S_HOUSE}, { 29,  2, S_HOUSE},
        { 2, 16, S_HOUSE}, { 30, 17, S_HOUSE}, { 16, 28, S_HOUSE},
        { 1, 29, S_CAMP },
        { 15, 15, S_WELL},                     // village centre
    };
    const int ccx = ox + 17, ccz = oz + 17;    // plaza / well centre

    // Gravel path between two points, following the terrain surface.
    auto path = [&](int x0, int z0, int x1, int z1) {
        int steps = (abs(x1-x0) > abs(z1-z0)) ? abs(x1-x0) : abs(z1-z0);
        if (steps < 1) return;
        for (int s = 0; s <= steps; s++) {
            int px = x0 + (x1-x0)*s/steps;
            int pz = z0 + (z1-z0)*s/steps;
            int py = WorldGen::terrainHeight(px, pz, seed);
            if (py <= WATER_LEVEL) continue;
            put(px,   py, pz,   BLOCK_GRAVEL);
            put(px,   py, pz+1, BLOCK_GRAVEL);          // 2 wide

        }
    };

    uint32_t vh = hash2(ox, oz, seed + 5501u);
    for (int i = 0; i < (int)(sizeof(LOTS)/sizeof(LOTS[0])); i++) {
        // Skip a couple of lots per village for variety (but always the well).
        if (LOTS[i].type != S_WELL && ((vh >> i) & 7) == 0) continue;
        int bx = ox + LOTS[i].dx, bz = oz + LOTS[i].dz;
        int by = WorldGen::terrainHeight(bx, bz, seed);
        if (by <= WATER_LEVEL + 1) continue;            // don't build in water
        switch (LOTS[i].type) {
            case S_HOUSE: buildHouse(bx, by, bz, seed, put); break;
            case S_WELL:  buildWell (bx, by, bz, seed, put); break;
            case S_CAMP:  buildCamp (bx, by, bz, seed, put); break;
            default: break;
        }
        if (LOTS[i].type != S_WELL) path(bx+2, bz+2, ccx, ccz);   // road to centre
    }
}

// ── TempleGenerator ──────────────────────────────────────────────────────────
// Desert stepped pyramid (sandstone-style using sand + clay banding) with a
// levelled foundation, a hollow inner chamber, an entrance and a loot chest.
template<class F> void buildTemple(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int B = 9;                                    // base width
    // Foundation: bury a couple of blocks into the dunes so it never floats.
    for (int dx = 0; dx < B; dx++)
    for (int dz = 0; dz < B; dz++)
        for (int fy = oy - 2; fy <= oy; fy++) put(ox+dx, fy, oz+dz, BLOCK_SAND);

    // Stepped pyramid body (each level a smaller solid square).
    for (int L = 0; ; L++) {
        int lo = L, hi = B - 1 - L;
        if (lo > hi) break;
        int y = oy + 1 + L;
        for (int dx = lo; dx <= hi; dx++)
        for (int dz = lo; dz <= hi; dz++)
            put(ox+dx, y, oz+dz, (L & 1) ? BLOCK_CLAY : BLOCK_SAND);   // banding
    }

    // Hollow inner chamber at the base.
    for (int dx = 2; dx <= B-3; dx++)
    for (int dz = 2; dz <= B-3; dz++)
    for (int cy = oy + 1; cy <= oy + 2; cy++)
        put(ox+dx, cy, oz+dz, BLOCK_AIR);
    for (int dx = 2; dx <= B-3; dx++)                   // chamber floor
    for (int dz = 2; dz <= B-3; dz++)
        put(ox+dx, oy, oz+dz, BLOCK_COBBLESTONE);

    // Entrance on the -Z side.
    put(ox+B/2, oy+1, oz+2, BLOCK_AIR);
    put(ox+B/2, oy+2, oz+2, BLOCK_AIR);

    // Loot + a glass skylight.
    put(ox+B/2, oy+1, oz+B/2, BLOCK_CHEST);
    put(ox+B/2, oy+1 + (B-1)/2, oz+B/2, BLOCK_GLASS);
}

// ── DungeonGenerator (underground) ───────────────────────────────────────────
template<class F> void buildDungeon(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int W = 7, D = 7, H = 5;
    for (int dx = 0; dx < W; dx++)
    for (int dz = 0; dz < D; dz++)
    for (int dy = 0; dy < H; dy++) {
        bool shell = (dx==0||dx==W-1||dz==0||dz==D-1||dy==0||dy==H-1);
        put(ox+dx, oy+dy, oz+dz, shell ? BLOCK_COBBLESTONE : BLOCK_AIR); // carve room
    }
    put(ox+1,   oy+1, oz+1,   BLOCK_CHEST);                    // loot
    put(ox+W-2, oy+1, oz+D-2, BLOCK_CHEST);
    put(ox+1,   oy+1, oz+D-2, BLOCK_TORCH);                    // lit dungeon
    put(ox+W-2, oy+1, oz+1,   BLOCK_TORCH);
}

// ── MineGenerator (underground tunnels + supports) ───────────────────────────
template<class F> void buildMine(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int L = 22;
    for (int d = 0; d < L; d++) {                              // 3×3 tunnel along X
        for (int dy = 0; dy < 3; dy++)
        for (int dz = 0; dz < 3; dz++)
            put(ox+d, oy+dy, oz+dz, BLOCK_AIR);                // carve
        put(ox+d, oy-1, oz,   BLOCK_PLANKS);                   // floor boards
        put(ox+d, oy-1, oz+1, BLOCK_PLANKS);
        put(ox+d, oy-1, oz+2, BLOCK_PLANKS);
        if (d % 4 == 0) {                                      // wood supports
            put(ox+d, oy,   oz,   BLOCK_WOOD); put(ox+d, oy+1, oz,   BLOCK_WOOD);
            put(ox+d, oy,   oz+2, BLOCK_WOOD); put(ox+d, oy+1, oz+2, BLOCK_WOOD);
            put(ox+d, oy+2, oz,   BLOCK_WOOD); put(ox+d, oy+2, oz+1, BLOCK_WOOD);
            put(ox+d, oy+2, oz+2, BLOCK_WOOD);
        }
        if (d % 8 == 5) put(ox+d, oy+1, oz+1, BLOCK_TORCH);    // lighting
    }
    put(ox+L-2, oy, oz+1, BLOCK_CHEST);                        // minecart loot
}

// ── CastleGenerator (large surface keep) ─────────────────────────────────────
template<class F> void buildCastle(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int B = 15, WALL = 5;
    for (int dx = 0; dx < B; dx++)                             // curtain wall
    for (int dz = 0; dz < B; dz++) {
        bool edge = (dx==0||dx==B-1||dz==0||dz==B-1);
        put(ox+dx, oy, oz+dz, BLOCK_STONE);                   // courtyard floor
        if (!edge) continue;
        for (int dy = 1; dy <= WALL; dy++) {
            if (dx==B/2 && dz==0 && dy<=3) continue;           // gate
            uint8_t b = (dy==WALL && ((dx+dz)&1)) ? BLOCK_AIR : BLOCK_COBBLESTONE; // crenellations
            if (b) put(ox+dx, oy+dy, oz+dz, b);
        }
    }
    int cpx[4] = {0, B-1, 0, B-1}, cpz[4] = {0, 0, B-1, B-1};  // 4 corner towers
    for (int c = 0; c < 4; c++)
        for (int dy = 1; dy <= WALL+3; dy++)
            put(ox+cpx[c], oy+dy, oz+cpz[c], BLOCK_COBBLESTONE);
    for (int dx = B/2-2; dx <= B/2+2; dx++)                    // central keep
    for (int dz = B/2-2; dz <= B/2+2; dz++)
    for (int dy = 1; dy <= WALL+2; dy++) {
        bool edge = (dx==B/2-2||dx==B/2+2||dz==B/2-2||dz==B/2+2);
        if (edge) put(ox+dx, oy+dy, oz+dz, BLOCK_STONE);
    }
    put(ox+B/2, oy+1, oz+B/2, BLOCK_CHEST);
    put(ox+2,   oy+1, oz+2,   BLOCK_TORCH); put(ox+B-3, oy+1, oz+B-3, BLOCK_TORCH);
}

// ── FortressGenerator (dark stronghold) ──────────────────────────────────────
template<class F> void buildFortress(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int B = 13, WALL = 6;
    for (int dx = 0; dx < B; dx++)
    for (int dz = 0; dz < B; dz++) {
        bool edge = (dx==0||dx==B-1||dz==0||dz==B-1);
        put(ox+dx, oy, oz+dz, BLOCK_COBBLESTONE);
        if (!edge) continue;
        for (int dy = 1; dy <= WALL; dy++) {
            if (dx==B/2 && dz==0 && dy<=3) continue;           // arch
            uint8_t b = (dy % 3 == 0) ? BLOCK_OBSIDIAN : BLOCK_COBBLESTONE; // dark banding
            put(ox+dx, oy+dy, oz+dz, b);
        }
    }
    for (int c = 0; c < 4; c++) {                              // obsidian corner pillars
        int px = (c&1)?B-1:0, pz = (c&2)?B-1:0;
        for (int dy = 1; dy <= WALL+2; dy++) put(ox+px, oy+dy, oz+pz, BLOCK_OBSIDIAN);
    }
    put(ox+B/2, oy+1, oz+B/2, BLOCK_CHEST);
    put(ox+B/2, oy+1, oz+B/2-2, BLOCK_TORCH);
}

// ── MonolithGenerator (giant standing stones) ────────────────────────────────
// Two or three rough rock spires, each anchored to its own ground height so
// the group can stand on a slope like a natural formation.
template<class F> void buildMonolith(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int n = 2 + (int)(hash2(ox, oz, seed + 301u) % 2);
    for (int i = 0; i < n; i++) {
        uint32_t h = hash2(ox + i * 17, oz + i * 31, seed + 313u);
        const int bx = ox + (int)(h % 5);
        const int bz = oz + (int)((h >> 4) % 5);
        const int tall = 5 + (int)((h >> 8) % 5);
        const int by = WorldGen::terrainHeight((float)bx, (float)bz, seed);
        if (by <= WATER_LEVEL) continue;
        for (int dy = 1; dy <= tall; dy++) {
            const uint8_t rock = ((h >> dy) & 3) ? BLOCK_STONE : BLOCK_COBBLESTONE;
            put(bx, by + dy, bz, rock);
            if (dy <= tall - 3) {                       // thicker weathered base
                put(bx + 1, by + dy, bz, rock);
                put(bx, by + dy, bz + 1, rock);
            }
        }
    }
}

// ── ArchGenerator (natural stone bridge) ─────────────────────────────────────
template<class F> void buildArch(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int L = 9, H = 5;
    for (int dz = 0; dz < 2; dz++) {
        for (int dy = 1; dy <= H; dy++) {               // two thick legs
            put(ox,     oy + dy, oz + dz, BLOCK_STONE);
            put(ox + 1, oy + dy, oz + dz, BLOCK_STONE);
            put(ox + L - 1, oy + dy, oz + dz, BLOCK_STONE);
            put(ox + L,     oy + dy, oz + dz, BLOCK_STONE);
        }
        for (int d = 0; d <= L; d++) {                  // curved span
            const float t = (2.0f * d / L) - 1.0f;
            const int lift = (int)(2.2f * (1.0f - t * t) + 0.5f);
            put(ox + d, oy + H + lift, oz + dz, BLOCK_STONE);
            if (lift > 0)
                put(ox + d, oy + H + lift - 1, oz + dz, BLOCK_STONE);
        }
    }
}

// ── GreatTreeGenerator (landmark tree, rare golden variant) ──────────────────
template<class F> void buildGreatTree(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int cx = ox + 6, cz = oz + 6, TH = 11;
    const uint32_t th = hash2(ox, oz, seed + 401u);
    const uint8_t leaf = (th % 10) < 3 ? BLOCK_LEAVES_AUTUMN : BLOCK_LEAVES;

    for (int dy = 1; dy <= TH; dy++)                    // 2×2 trunk
    for (int dx = 0; dx < 2; dx++)
    for (int dz = 0; dz < 2; dz++)
        put(cx + dx, oy + dy, cz + dz, BLOCK_WOOD);

    for (int b = 0; b < 4; b++) {                       // four thick branches
        const int sx = (b & 1) ? 1 : -1, sz = (b & 2) ? 1 : -1;
        for (int e = 1; e <= 3; e++)
            put(cx + (sx > 0 ? 1 : 0) + sx * e,
                oy + TH - 3 + e / 2,
                cz + (sz > 0 ? 1 : 0) + sz * e, BLOCK_WOOD);
    }

    for (int dx = -5; dx <= 5; dx++)                    // huge ellipsoid canopy
    for (int dy = -2; dy <= 3; dy++)
    for (int dz = -5; dz <= 5; dz++) {
        const float d = (float)(dx * dx + dz * dz) + (float)(dy * dy) * 3.2f;
        if (d > 27.0f) continue;
        if (d > 18.0f &&
            hash2(cx + dx * 13 + dy, cz + dz * 11 - dy, seed + 431u) % 100 > 82)
            continue;                                   // ragged edge
        put(cx + dx, oy + TH + dy, cz + dz, leaf);
    }
}

// ── PortalGenerator (obsidian frame) ─────────────────────────────────────────
template<class F> void buildPortal(int ox, int oy, int oz, unsigned seed, F&& put) {
    const int W = 4, H = 5;                                    // frame in the X-Y plane
    for (int dy = 0; dy < H; dy++) { put(ox, oy+dy, oz, BLOCK_OBSIDIAN); put(ox+W-1, oy+dy, oz, BLOCK_OBSIDIAN); }
    for (int dx = 0; dx < W; dx++) { put(ox+dx, oy, oz, BLOCK_OBSIDIAN); put(ox+dx, oy+H-1, oz, BLOCK_OBSIDIAN); }
    for (int dx = 1; dx < W-1; dx++)
    for (int dy = 1; dy < H-1; dy++)
        put(ox+dx, oy+dy, oz, BLOCK_GLASS);                    // portal surface
    put(ox+1, oy+1, oz+1, BLOCK_TORCH);
}

template<class F>
void build(Type t, int ox, int oy, int oz, unsigned seed, F&& put) {
    switch (t) {
        case S_CABIN:  buildCabin (ox, oy, oz, seed, put); break;
        case S_HOUSE:  buildHouse (ox, oy, oz, seed, put); break;
        case S_RUINS:  buildRuins (ox, oy, oz, seed, put); break;
        case S_TOWER:  buildTower (ox, oy, oz, seed, put); break;
        case S_WELL:   buildWell  (ox, oy, oz, seed, put); break;
        case S_CAMP:   buildCamp  (ox, oy, oz, seed, put); break;
        case S_BRIDGE: buildBridge(ox, oy, oz, seed, put); break;
        case S_VILLAGE:buildVillage(ox, oz, seed, put);    break;   // self-adapts height
        case S_TEMPLE: buildTemple(ox, oy, oz, seed, put); break;
        case S_DUNGEON:  buildDungeon (ox, oy, oz, seed, put); break;
        case S_MINE:     buildMine    (ox, oy, oz, seed, put); break;
        case S_CASTLE:   buildCastle  (ox, oy, oz, seed, put); break;
        case S_FORTRESS: buildFortress(ox, oy, oz, seed, put); break;
        case S_PORTAL:   buildPortal  (ox, oy, oz, seed, put); break;
        case S_MONOLITH: buildMonolith(ox, oy, oz, seed, put); break;
        case S_ARCH:     buildArch    (ox, oy, oz, seed, put); break;
        case S_GREATTREE:buildGreatTree(ox, oy, oz, seed, put); break;
        default: break;                                   // reserved types: no-op
    }
}

// ── StructureManager: stamp all structures overlapping chunk (cx,cz) ─────────
// Returns the number of structures whose ORIGIN lies in this chunk (so summing
// over chunks counts each structure once); *outType gets the last such type.
inline int generate(uint8_t blocks[CHUNK_W][CHUNK_H][CHUNK_D],
                    int cx, int cz, unsigned seed, uint8_t* outType = nullptr) {
    const int cwx0 = cx * CHUNK_W, cwz0 = cz * CHUNK_D;
    auto put = [&](int wx, int wy, int wz, uint8_t block) {
        int lx = wx - cwx0, lz = wz - cwz0;
        if (lx < 0 || lx >= CHUNK_W || lz < 0 || lz >= CHUNK_D) return;
        if (wy < 1 || wy >= CHUNK_H) return;              // never touch bedrock/void
        blocks[lx][wy][lz] = block;
    };

    const int MAXFOOT = 40;                               // covers the largest (village)
    int gx0 = (cwx0 - MAXFOOT) / CELL - 1, gx1 = (cwx0 + CHUNK_W + MAXFOOT) / CELL + 1;
    int gz0 = (cwz0 - MAXFOOT) / CELL - 1, gz1 = (cwz0 + CHUNK_D + MAXFOOT) / CELL + 1;

    int originCount = 0;
    for (int gx = gx0; gx <= gx1; gx++)
    for (int gz = gz0; gz <= gz1; gz++) {
        int ox, oz;
        Type t = cellPick(gx, gz, seed, ox, oz);
        if (t == S_NONE) continue;

        const Def& d = def(t);
        int fp = d.footprint;

        // Skip early when the structure cannot touch this chunk: builders and
        // their terrain probes are expensive (village paths sample the height
        // field per block), and most candidate cells lie entirely outside.
        // The mine's tunnel runs past its nominal footprint; roofs/canopies
        // overhang up to 2 blocks.
        const int extent = (t == S_MINE ? 24 : fp) + 2;
        if (ox + extent < cwx0 || ox - 2 >= cwx0 + CHUNK_W ||
            oz + extent < cwz0 || oz - 2 >= cwz0 + CHUNK_D) continue;

        int oy;

        if (isUnderground(t)) {
            // Buried well below the surface, inside the rock.
            int surf = WorldGen::terrainHeight(ox + fp/2, oz + fp/2, seed);
            if (surf <= WATER_LEVEL + 1) continue;        // don't dig under water
            oy = surf - (t == S_DUNGEON ? 12 : 16);
            if (oy < 6) continue;                          // too close to bedrock
        } else if (fp <= 12) {
            // Small builds require flat-ish ground (no weird mountain cuts).
            int hc[4] = {
                WorldGen::terrainHeight(ox,      oz,      seed),
                WorldGen::terrainHeight(ox+fp-1, oz,      seed),
                WorldGen::terrainHeight(ox,      oz+fp-1, seed),
                WorldGen::terrainHeight(ox+fp-1, oz+fp-1, seed),
            };
            int hmin = hc[0], hmax = hc[0];
            for (int i = 1; i < 4; i++) { if (hc[i]<hmin) hmin=hc[i]; if (hc[i]>hmax) hmax=hc[i]; }
            if (hmin <= WATER_LEVEL + 1) continue;        // avoid oceans/rivers/lakes
            if (hmax - hmin > 3)         continue;        // adapt to terrain / no cuts
            oy = hmin;
        } else {
            // Large builds (village/temple) self-adapt; just avoid deep water.
            oy = WorldGen::terrainHeight(ox + fp/2, oz + fp/2, seed);
            if (oy <= WATER_LEVEL + 1) continue;
        }

        build(t, ox, oy, oz, seed, put);

        // Count only if the origin is inside this chunk (dedupe across chunks).
        if (ox >= cwx0 && ox < cwx0 + CHUNK_W && oz >= cwz0 && oz < cwz0 + CHUNK_D) {
            originCount++;
            if (outType) *outType = (uint8_t)t;
        }
    }
    return originCount;
}

} // namespace Structures
