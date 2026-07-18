#include "texture_atlas.h"
#include "platform.h"
#include <vector>
#include <cstdint>
#include <cmath>

using namespace Atlas;

// ── Tiny deterministic per-pixel hash noise ─────────────────────────────────
static inline uint32_t h32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
static inline float pnoise(int x, int y, uint32_t s) {
    return (h32((uint32_t)x * 374761393U ^ (uint32_t)y * 668265263U ^ s * 2246822519U)
            & 0xFFFF) / 65535.0f;
}
static inline int clampb(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static std::vector<unsigned char> g_buf;

static inline void setPx(int x, int y, int r, int g, int b, int a) {
    int i = (y * ATLAS_PX + x) * 4;
    g_buf[i+0] = (unsigned char)clampb(r);
    g_buf[i+1] = (unsigned char)clampb(g);
    g_buf[i+2] = (unsigned char)clampb(b);
    g_buf[i+3] = (unsigned char)clampb(a);
}

static void tileCell(uint16_t tile, int& x0, int& y0) {
    x0 = (tile % ATLAS_COLS) * TILE_STRIDE;
    y0 = (tile / ATLAS_COLS) * TILE_STRIDE;
}

// Flat speckled block (dirt/stone/sand/…). `jit` = brightness jitter amplitude.
// Two-octave speckle (coarse patches + fine grain) plus a whisper of vertical
// gradient so faces never read as perfectly flat colour.
static void paintSolid(uint16_t tile, int R, int G, int B, int jit, uint32_t seed) {
    int x0, y0; tileCell(tile, x0, y0);
    for (int ly = 0; ly < TILE_PX; ly++)
    for (int lx = 0; lx < TILE_PX; lx++) {
        const float coarse = pnoise(lx / 4, ly / 4, seed + 91u) - 0.5f;
        const float fine   = pnoise(lx, ly, seed) - 0.5f;
        int d = (int)(coarse * jit * 0.6f + fine * jit * 0.7f);
        d += (TILE_PX / 2 - ly) * 6 / TILE_PX;          // subtle top light
        setPx(x0+lx, y0+ly, R+d, G+d, B+d, 255);
    }
}

// Same but with explicit alpha (transparent water).
static void paintSolidA(uint16_t tile, int R, int G, int B, int jit,
                        uint32_t seed, int alpha) {
    paintSolid(tile, R, G, B, jit, seed);
    int x0, y0; tileCell(tile, x0, y0);
    for (int ly = 0; ly < TILE_PX; ly++)
    for (int lx = 0; lx < TILE_PX; lx++) {
        const int i = ((y0+ly) * ATLAS_PX + (x0+lx)) * 4;
        g_buf[i+3] = (unsigned char)alpha;
    }
}

// Stamp a few coloured ore blobs over an already-painted base tile.
static void paintOre(uint16_t tile, int oreR, int oreG, int oreB, uint32_t seed) {
    int x0, y0; tileCell(tile, x0, y0);
    for (int b = 0; b < 9; b++) {
        int bx = 3 + (int)(pnoise(b, 0, seed) * (TILE_PX - 9));
        int by = 3 + (int)(pnoise(0, b, seed) * (TILE_PX - 9));
        const int sz = 3 + (b & 1) * 2;
        for (int dy = 0; dy < sz; dy++)
        for (int dx = 0; dx < sz; dx++) {
            if ((dx == 0 || dx == sz-1) && (dy == 0 || dy == sz-1)) continue; // round corners
            int d = (int)((pnoise(bx+dx, by+dy, seed) - 0.5f) * 40);
            setPx(x0 + bx + dx, y0 + by + dy, oreR+d, oreG+d, oreB+d, 255);
        }
    }
}

// Fill a cross-plant / cutout tile with transparent background first.
static void clearTile(uint16_t tile) {
    int x0, y0; tileCell(tile, x0, y0);
    for (int ly = 0; ly < TILE_PX; ly++)
    for (int lx = 0; lx < TILE_PX; lx++) setPx(x0+lx, y0+ly, 0, 0, 0, 0);
}

// Small pixel-art detail passes layered over the flat speckle base.
static inline int cl255(float v) { return v < 0 ? 0 : (v > 255 ? 255 : (int)v); }

static void grassClumps(uint16_t tile, int R, int G, int B, uint32_t seed) {
    int x0, y0; tileCell(tile, x0, y0);
    for (int ly = 0; ly < TILE_PX; ly++)
    for (int lx = 0; lx < TILE_PX; lx++) {
        const float n = pnoise(lx, ly, seed);
        if (n > 0.91f) {                                   // dark tuft (3px tall)
            setPx(x0+lx, y0+ly, cl255(R*0.70f), cl255(G*0.74f), cl255(B*0.70f), 255);
            if (ly + 1 < TILE_PX)
                setPx(x0+lx, y0+ly+1, cl255(R*0.78f), cl255(G*0.80f), cl255(B*0.76f), 255);
            if (ly + 2 < TILE_PX)
                setPx(x0+lx, y0+ly+2, cl255(R*0.86f), cl255(G*0.88f), cl255(B*0.84f), 255);
        } else if (n < 0.05f) {                            // sunlit blade tip
            setPx(x0+lx, y0+ly, cl255(R*1.35f), cl255(G*1.26f), cl255(B*1.35f), 255);
            if (ly + 1 < TILE_PX)
                setPx(x0+lx, y0+ly+1, cl255(R*1.18f), cl255(G*1.14f), cl255(B*1.18f), 255);
        }
    }
}

// White dusting over an already-painted tile (winter). Skips transparent
// pixels so bare-canopy holes stay holes.
static void dustSnow(uint16_t tile, float amount, uint32_t seed) {
    if (amount <= 0.01f) return;
    int x0, y0; tileCell(tile, x0, y0);
    for (int ly = 0; ly < TILE_PX; ly++)
    for (int lx = 0; lx < TILE_PX; lx++) {
        const int i = ((y0+ly) * ATLAS_PX + (x0+lx)) * 4;
        if (g_buf[i+3] == 0) continue;                      // keep cutout holes
        // Denser on the upper half of the tile: snow settles on top.
        const float bias = 1.25f - (float)ly / (float)TILE_PX;
        if (pnoise(lx, ly, seed) < amount * 0.55f * bias) {
            const int d = (int)((pnoise(lx+7, ly+3, seed) - 0.5f) * 18);
            setPx(x0+lx, y0+ly, 232+d, 238+d, 248+d, 255);
        }
    }
}

static void stoneCracks(uint16_t tile, uint32_t seed) {
    int x0, y0; tileCell(tile, x0, y0);
    // Two short random-walk cracks per tile.
    for (int c = 0; c < 2; c++) {
        int px = 2 + (int)(pnoise(c, 0, seed) * (TILE_PX - 4));
        int py = 1 + (int)(pnoise(0, c, seed) * 3);
        for (int s = 0; s < TILE_PX - 3 && py < TILE_PX; s++) {
            setPx(x0+px, y0+py, 84, 84, 90, 255);
            py++;
            const float w = pnoise(px, py + s, seed + 7u);
            if (w > 0.66f && px + 1 < TILE_PX) px++;
            else if (w < 0.33f && px > 0) px--;
        }
    }
}

static void sandRipples(uint16_t tile, uint32_t seed) {
    int x0, y0; tileCell(tile, x0, y0);
    for (int ly = 2; ly < TILE_PX; ly += 5)
    for (int lx = 0; lx < TILE_PX; lx++) {
        const int wobble = (int)(pnoise(lx / 2, ly, seed) * 2.0f);
        const int ry = ly + wobble;
        if (ry < TILE_PX) setPx(x0+lx, y0+ry, 202, 184, 128, 255);
    }
}

// Scattered two-tone pebbles (dirt inclusions, gravel stones).
static void pebbles(uint16_t tile, int r, int g, int b, int count, uint32_t seed) {
    int x0, y0; tileCell(tile, x0, y0);
    for (int p = 0; p < count; p++) {
        const int px = 1 + (int)(pnoise(p, 3, seed) * (TILE_PX - 3));
        const int py = 1 + (int)(pnoise(5, p, seed) * (TILE_PX - 3));
        const int d = (int)((pnoise(px, py, seed) - 0.5f) * 26);
        setPx(x0+px,   y0+py,   r+d, g+d, b+d, 255);
        setPx(x0+px+1, y0+py,   r+d-8, g+d-8, b+d-8, 255);
        if (pnoise(p, 9, seed) > 0.5f)
            setPx(x0+px, y0+py+1, r+d-4, g+d-4, b+d-4, 255);
    }
}

// Diagonal streaks (ice cracks, clay strata).
static void streaks(uint16_t tile, int r, int g, int b, uint32_t seed) {
    int x0, y0; tileCell(tile, x0, y0);
    for (int s = 0; s < 3; s++) {
        int px = (int)(pnoise(s, 1, seed) * TILE_PX);
        int py = 0;
        while (py < TILE_PX) {
            setPx(x0 + (px % TILE_PX + TILE_PX) % TILE_PX, y0+py, r, g, b, 255);
            py++;
            if (pnoise(px, py + s * 7, seed) > 0.4f) px++;
        }
    }
}

static void snowSparkle(uint16_t tile, uint32_t seed) {
    int x0, y0; tileCell(tile, x0, y0);
    for (int ly = 0; ly < TILE_PX; ly++)
    for (int lx = 0; lx < TILE_PX; lx++) {
        const float n = pnoise(lx, ly, seed);
        if (n > 0.95f)      setPx(x0+lx, y0+ly, 255, 255, 255, 255); // sparkle
        else if (n < 0.06f) setPx(x0+lx, y0+ly, 208, 220, 238, 255); // blue shadow
    }
}

// ── Foliage (cutout) — seasonal ─────────────────────────────────────────────
// Two-tone leaf mass: base speckle + shadow pockets + sunlit tips, cutout
// holes for a ragged silhouette. holeExtra bares the canopy (winter),
// blossom sprinkles spring flowers, snow dust is applied by the caller.
static void paintFoliage(uint16_t tile, int r, int g, int b, uint32_t seed,
                         float holeExtra, float blossom) {
    paintSolid(tile, r, g, b, 34, seed);
    int x0, y0; tileCell(tile, x0, y0);
    for (int ly = 0; ly < TILE_PX; ly++)
    for (int lx = 0; lx < TILE_PX; lx++) {
        const float n = pnoise(lx, ly, seed + 1u);
        if (n < 0.14f + holeExtra)
            setPx(x0+lx, y0+ly, 0, 0, 0, 0);
        else if (n < 0.32f + holeExtra)
            setPx(x0+lx, y0+ly, r*2/3, g*2/3, b*2/3, 255);   // shadow pocket
        else if (n > 0.92f)
            setPx(x0+lx, y0+ly, r + (255-r)/4, g + (255-g)/4, b + (255-b)/4, 255);
        else if (blossom > 0.01f && pnoise(lx, ly, seed + 5u) > 1.0f - 0.10f * blossom)
            setPx(x0+lx, y0+ly, 238, 176, 200, 255);          // spring blossom
    }
}

// Repaints every season-dependent tile from a SeasonTint. Called at startup
// (summer keyframe) and by applySeason() when the year drifts.
static void paintSeasonTiles(const SN::SeasonTint& t) {
    const int gr = (int)t.grassR, gg = (int)t.grassG, gb = (int)t.grassB;

    // Grass tops: three variants around the seasonal base colour.
    paintSolid(T_GRASS_TOP0, gr, gg, gb, 28, 11);
    paintSolid(T_GRASS_TOP1, cl255(gr*1.06f), cl255(gg*1.07f), cl255(gb*1.05f), 30, 23);
    paintSolid(T_GRASS_TOP2, cl255(gr*0.93f), cl255(gg*0.94f), cl255(gb*0.91f), 26, 37);
    grassClumps(T_GRASS_TOP0, gr, gg, gb, 411u);
    grassClumps(T_GRASS_TOP1, gr, gg, gb, 423u);
    grassClumps(T_GRASS_TOP2, gr, gg, gb, 437u);
    dustSnow(T_GRASS_TOP0, t.snowDust, 611u);
    dustSnow(T_GRASS_TOP1, t.snowDust, 613u);
    dustSnow(T_GRASS_TOP2, t.snowDust, 617u);

    // Grass side: dirt base + seasonal fringe.
    {
        paintSolid(T_GRASS_SIDE, 134, 96, 62, 24, 71);
        int x0, y0; tileCell(T_GRASS_SIDE, x0, y0);
        for (int lx = 0; lx < TILE_PX; lx++) {
            int edge = 6 + (int)(pnoise(lx, 99, 5u) * 6);
            for (int ly = 0; ly < edge; ly++) {
                int d = (int)((pnoise(lx, ly, 12u) - 0.5f) * 28);
                setPx(x0+lx, y0+ly, gr+d, gg+d, gb+d, 255);
            }
            // hanging root/blade tips under the fringe
            if (pnoise(lx, 7, 17u) > 0.7f && edge + 2 < TILE_PX)
                setPx(x0+lx, y0+edge, cl255(gr*0.8f), cl255(gg*0.8f), cl255(gb*0.8f), 255);
        }
        dustSnow(T_GRASS_SIDE, t.snowDust * 0.6f, 619u);
    }

    // Canopies. Pine keeps its needles year-round; the others follow the year.
    paintFoliage(T_LEAVES,        (int)t.oakR,   (int)t.oakG,   (int)t.oakB,   221u, t.holeExtra, t.blossom);
    paintFoliage(T_LEAVES_DARK,   38, 92, 46,                                  223u, 0.0f,        0.0f);
    paintFoliage(T_LEAVES_AUTUMN, (int)t.accR,   (int)t.accG,   (int)t.accB,   227u, t.holeExtra, t.blossom * 0.5f);
    paintFoliage(T_LEAVES_BIRCH,  (int)t.birchR, (int)t.birchG, (int)t.birchB, 229u, t.holeExtra, 0.0f);
    dustSnow(T_LEAVES,        t.snowDust, 631u);
    dustSnow(T_LEAVES_DARK,   t.snowDust, 641u);
    dustSnow(T_LEAVES_AUTUMN, t.snowDust, 643u);
    dustSnow(T_LEAVES_BIRCH,  t.snowDust, 647u);

    // Cross plants share the grass colour so meadows change together.
    {
        auto blades = [&](uint16_t tile, int R, int G, int B) {
            clearTile(tile);
            int x0, y0; tileCell(tile, x0, y0);
            for (int lx = 0; lx < TILE_PX; lx++) {
                if (pnoise(lx, 3, tile) < 0.45f) continue;
                int top = 6 + (int)(pnoise(lx, 8, tile) * 14);
                for (int ly = TILE_PX - 1; ly >= top; ly--) {
                    int d = (int)((pnoise(lx, ly, tile) - 0.5f) * 24);
                    d += (ly - top) / 3 - 3;                 // darker at the base
                    setPx(x0+lx, y0+ly, R+d, G+d, B+d, 255);
                }
            }
        };
        blades(T_TALL_GRASS, cl255(gr*0.86f), cl255(gg*0.98f), cl255(gb*0.94f));
        blades(T_BUSH,       cl255(gr*0.68f), cl255(gg*0.80f), cl255(gb*0.82f));
        dustSnow(T_TALL_GRASS, t.snowDust * 0.5f, 653u);
        dustSnow(T_BUSH,       t.snowDust * 0.5f, 659u);
    }

    // Vines: liana strands hanging from the tile top, with leaf pips. They
    // follow the oak tint so mangrove curtains change with the year.
    {
        clearTile(T_VINE);
        int x0, y0; tileCell(T_VINE, x0, y0);
        const int vr = cl255(t.oakR * 0.74f);
        const int vg = cl255(t.oakG * 0.90f);
        const int vb = cl255(t.oakB * 0.72f);
        for (int lx = 0; lx < TILE_PX; lx++) {
            if (pnoise(lx, 5, 761u) < 0.42f) continue;       // strand spacing
            const int len = 12 + (int)(pnoise(lx, 11, 769u) * (TILE_PX - 12));
            const int sway = pnoise(lx, 2, 773u) > 0.5f ? 1 : -1;
            for (int ly = 0; ly < len; ly++) {
                const int px = lx + ((ly / 10) & 1) * sway;
                if (px < 0 || px >= TILE_PX) break;
                const int d = (int)((pnoise(px, ly, 787u) - 0.5f) * 26);
                setPx(x0+px, y0+ly, vr+d, vg+d, vb+d, 255);
                if (pnoise(px, ly, 797u) > 0.80f && px + 1 < TILE_PX)
                    setPx(x0+px+1, y0+ly, vr+12, vg+18, vb+10, 255);
            }
        }
        dustSnow(T_VINE, t.snowDust * 0.5f, 661u);
    }

    // Water: translucent teal (the seabed shows through); freezes toward a
    // pale, more opaque ice with hairline cracks in deep winter.
    {
        const float k = t.ice;
        paintSolidA(T_WATER,
                    cl255(38.0f  + (172.0f - 38.0f)  * k),
                    cl255(104.0f + (208.0f - 104.0f) * k),
                    cl255(168.0f + (234.0f - 168.0f) * k),
                    (int)(18.0f - 6.0f * k), 261,
                    (int)(178.0f + 70.0f * k));
        // faint wave crests on liquid water
        if (k < 0.45f) {
            int x0, y0; tileCell(T_WATER, x0, y0);
            for (int ly = 3; ly < TILE_PX; ly += 9)
            for (int lx = 0; lx < TILE_PX; lx++)
                if (pnoise(lx / 3, ly, 663u) > 0.55f)
                    setPx(x0+lx, y0+ly + (int)(pnoise(lx/4, ly, 667u)*2),
                          92, 158, 210, 200);
        }
        if (k > 0.45f) streaks(T_WATER, 210, 232, 246, 661u);

        // Shoreline foam variant: same water + white bubble speckles. The
        // mesher picks this tile for water tops that touch land.
        {
            paintSolidA(T_WATER_FOAM,
                        cl255(52.0f  + (176.0f - 52.0f)  * k),
                        cl255(116.0f + (212.0f - 116.0f) * k),
                        cl255(176.0f + (238.0f - 176.0f) * k),
                        14, 261, (int)(196.0f + 55.0f * k));
            int x0, y0; tileCell(T_WATER_FOAM, x0, y0);
            for (int ly = 0; ly < TILE_PX; ly++)
            for (int lx = 0; lx < TILE_PX; lx++)
                if (pnoise(lx, ly, 671u) > 0.80f - 0.30f * pnoise(lx/5, ly/5, 673u))
                    setPx(x0+lx, y0+ly, 226, 240, 248, 235);
        }
    }
}

static void buildTiles() {
    // ── Terrain / stone ──────────────────────────────────────────────────
    paintSolid(T_DIRT0, 134, 96, 62, 26, 41);
    paintSolid(T_DIRT1, 128, 92, 58, 26, 53);
    paintSolid(T_DIRT2, 140, 100, 66, 26, 67);
    pebbles(T_DIRT0, 104, 74, 46, 12, 471u);
    pebbles(T_DIRT1, 100, 70, 44, 10, 473u);
    pebbles(T_DIRT2, 110, 78, 50, 14, 477u);

    paintSolid(T_STONE0, 122, 122, 126, 22, 101);
    paintSolid(T_STONE1, 116, 116, 120, 22, 103);
    paintSolid(T_STONE2, 128, 128, 132, 22, 107);
    stoneCracks(T_STONE0, 441u);
    stoneCracks(T_STONE1, 443u);
    stoneCracks(T_STONE2, 447u);

    // Cobblestone = stone with a blocky cell pattern
    {
        paintSolid(T_COBBLE, 120, 120, 124, 30, 131);
        int x0, y0; tileCell(T_COBBLE, x0, y0);
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++) {
            const bool mortar = (lx % 14 == 0) || (ly % 14 == 0) ||
                                ((lx + ly) % 11 == 0);
            if (mortar) setPx(x0+lx, y0+ly, 70, 70, 74, 255);
            else if ((lx % 14 == 1) || (ly % 14 == 1))          // bevel highlight
                setPx(x0+lx, y0+ly, 138, 138, 144, 255);
        }
    }

    paintSolid(T_SAND0, 224, 208, 152, 20, 151);
    paintSolid(T_SAND1, 218, 202, 146, 20, 157);
    sandRipples(T_SAND0, 451u);
    sandRipples(T_SAND1, 457u);
    paintSolid(T_GRAVEL0, 120, 112, 104, 34, 161);
    paintSolid(T_GRAVEL1, 128, 118, 110, 34, 167);
    pebbles(T_GRAVEL0, 92, 86, 80, 22, 481u);
    pebbles(T_GRAVEL0, 150, 142, 132, 16, 483u);
    pebbles(T_GRAVEL1, 98, 92, 86, 22, 487u);
    pebbles(T_GRAVEL1, 158, 148, 138, 16, 491u);
    paintSolid(T_BEDROCK, 46, 46, 50, 44, 181);
    streaks(T_BEDROCK, 24, 24, 28, 493u);
    paintSolid(T_DEEPSLATE0, 66, 66, 78, 22, 191);
    paintSolid(T_DEEPSLATE1, 60, 60, 72, 22, 193);

    // ── Wood ─────────────────────────────────────────────────────────────
    {
        paintSolid(T_WOOD_SIDE, 120, 84, 46, 18, 201);
        int x0, y0; tileCell(T_WOOD_SIDE, x0, y0);
        for (int lx = 0; lx < TILE_PX; lx++) {
            const float gv = pnoise(lx, 7, 202u);
            if (gv > 0.72f)                                 // deep grain lines
                for (int ly = 0; ly < TILE_PX; ly++) {
                    const int wob = (pnoise(lx, ly / 5, 204u) > 0.6f) ? 1 : 0;
                    setPx(x0 + ((lx + wob) % TILE_PX), y0+ly, 92, 62, 34, 255);
                }
            else if (gv < 0.16f)                            // light grain streak
                for (int ly = 0; ly < TILE_PX; ly++)
                    setPx(x0+lx, y0+ly, 136, 98, 56, 255);
        }
    }
    {
        paintSolid(T_WOOD_TOP, 150, 110, 66, 14, 211);
        int x0, y0; tileCell(T_WOOD_TOP, x0, y0);
        float c = (TILE_PX - 1) * 0.5f;
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++) {
            float r = sqrtf((lx-c)*(lx-c) + (ly-c)*(ly-c));
            if (((int)r) % 5 == 0)      setPx(x0+lx, y0+ly, 118, 84, 48, 255); // rings
            else if (((int)r) % 5 == 1) setPx(x0+lx, y0+ly, 158, 118, 72, 255);
        }
    }
    {
        // Birch bark: pale base with short dark horizontal lenticels
        paintSolid(T_WOOD_BIRCH_SIDE, 216, 214, 204, 12, 205);
        int x0, y0; tileCell(T_WOOD_BIRCH_SIDE, x0, y0);
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++)
            if (pnoise(lx / 6, ly / 2, 206u) > 0.84f)       // wide lenticels
                setPx(x0+lx, y0+ly, 54, 50, 44, 255);
    }
    {
        paintSolid(T_WOOD_BIRCH_TOP, 198, 188, 160, 12, 213);
        int x0, y0; tileCell(T_WOOD_BIRCH_TOP, x0, y0);
        float c = (TILE_PX - 1) * 0.5f;
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++) {
            float r = sqrtf((lx-c)*(lx-c) + (ly-c)*(ly-c));
            if (((int)r) % 5 == 0) setPx(x0+lx, y0+ly, 168, 156, 126, 255);
        }
    }

    // Foliage / grass tops / water are painted by paintSeasonTiles() below,
    // so the seasonal repaint and the startup build share one code path.

    paintSolid(T_SNOW, 236, 240, 248, 14, 231);
    snowSparkle(T_SNOW, 461u);
    paintSolid(T_ICE,  150, 194, 226, 12, 241);
    streaks(T_ICE, 196, 226, 246, 497u);
    paintSolid(T_CLAY, 156, 162, 176, 16, 251);
    pebbles(T_CLAY, 138, 144, 160, 4, 499u);

    // ── Ores (stone / deepslate base + coloured blobs) ───────────────────
    paintSolid(T_COAL,    120,120,126,18,101); paintOre(T_COAL,    38, 38, 42, 271);
    paintSolid(T_IRON,    120,120,126,18,101); paintOre(T_IRON,    198,150,110, 277);
    paintSolid(T_COPPER,  120,120,126,18,101); paintOre(T_COPPER,  196,110, 70, 281);
    paintSolid(T_GOLD,    120,120,126,18,101); paintOre(T_GOLD,    224,192, 70, 283);
    paintSolid(T_DIAMOND, 66,66,78,18,191);    paintOre(T_DIAMOND, 110,224,224, 287);
    paintSolid(T_EMERALD, 66,66,78,18,191);    paintOre(T_EMERALD, 60,200,110, 293);

    // ── Cross plants (cutout shapes on transparent) ──────────────────────
    // (tall grass + bush are seasonal → painted in paintSeasonTiles)
    auto flower = [&](uint16_t tile, int R, int G, int B) {
        clearTile(tile);
        int x0, y0; tileCell(tile, x0, y0);
        int cx = TILE_PX/2;
        for (int ly = TILE_PX-1; ly >= TILE_PX/2 - 2; ly--) { // green stem (2px)
            setPx(x0+cx,   y0+ly, 60, 120, 46, 255);
            setPx(x0+cx-1, y0+ly, 48, 102, 38, 255);
        }
        setPx(x0+cx+2, y0+TILE_PX*3/4, 60, 120, 46, 255);    // leaf on the stem
        setPx(x0+cx+3, y0+TILE_PX*3/4 - 1, 60, 120, 46, 255);
        for (int dy = 4; dy < 12; dy++)                      // coloured head
        for (int dx = -4; dx <= 4; dx++)
            if (dx*dx + (dy-8)*(dy-8) <= 18) {
                const bool rim = dx*dx + (dy-8)*(dy-8) > 10;
                setPx(x0+cx+dx, y0+dy,
                      rim ? R : cl255(R*1.2f),
                      rim ? G : cl255(G*1.2f),
                      rim ? B : cl255(B*1.2f), 255);
            }
        setPx(x0+cx, y0+8, 255, 232, 128, 255);              // centre
    };
    flower(T_FLOWER_RED,    210, 60, 60);
    flower(T_FLOWER_YELLOW, 226, 200, 60);

    auto mushroom = [&](uint16_t tile, int R, int G, int B) {
        clearTile(tile);
        int x0, y0; tileCell(tile, x0, y0);
        int cx = TILE_PX/2;
        for (int ly = TILE_PX-1; ly >= TILE_PX/2 + 2; ly--) { // pale stem (2px)
            setPx(x0+cx,   y0+ly, 214, 206, 190, 255);
            setPx(x0+cx-1, y0+ly, 196, 188, 172, 255);
        }
        for (int dy = 6; dy <= 14; dy++)                      // domed cap
        for (int dx = -6; dx <= 6; dx++)
            if (dy - 6 >= abs(dx) - 2) {
                const bool spot = ((dx + dy) % 5 == 0) && dy < 12;
                setPx(x0+cx+dx, y0+dy,
                      spot ? 244 : R, spot ? 240 : G, spot ? 228 : B, 255);
            }
    };
    mushroom(T_MUSH_RED,   200, 50, 50);
    mushroom(T_MUSH_BROWN, 150, 110, 78);

    paintSolid(T_ROCK, 110, 110, 114, 26, 301);   // small solid rock
    stoneCracks(T_ROCK, 503u);
    pebbles(T_ROCK, 88, 88, 94, 4, 509u);

    // ── Structure blocks ─────────────────────────────────────────────────
    {
        paintSolid(T_PLANKS, 178, 140, 88, 16, 311);
        int x0, y0; tileCell(T_PLANKS, x0, y0);
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++) {
            if (ly % 7 == 0 || (lx % 14 == (ly / 7) * 5 % 14))  // plank seams
                setPx(x0+lx, y0+ly, 138, 104, 62, 255);
            else if (ly % 7 == 1)                               // seam highlight
                setPx(x0+lx, y0+ly, 194, 156, 100, 255);
        }
    }
    {
        paintSolid(T_GLASS, 200, 224, 236, 6, 321);
        int x0, y0; tileCell(T_GLASS, x0, y0);
        for (int b = 0; b < 2; b++)                    // 2px pane frame
        for (int lx = 0; lx < TILE_PX; lx++) {
            setPx(x0+lx, y0+b, 150, 170, 182, 255);
            setPx(x0+lx, y0+TILE_PX-1-b, 150, 170, 182, 255);
            setPx(x0+b, y0+lx, 150, 170, 182, 255);
            setPx(x0+TILE_PX-1-b, y0+lx, 150, 170, 182, 255);
        }
        for (int d = 3; d < 10; d++)                   // diagonal sheen
            setPx(x0+d, y0+12-d, 236, 248, 252, 255);
    }
    {
        paintSolid(T_CHEST_TOP, 150, 108, 60, 14, 331);
        int x0, y0; tileCell(T_CHEST_TOP, x0, y0);
        for (int lx = 4; lx < TILE_PX-4; lx++) {      // lid rim (2px)
            setPx(x0+lx, y0+4, 96, 66, 34, 255);
            setPx(x0+lx, y0+5, 96, 66, 34, 255);
            setPx(x0+lx, y0+TILE_PX-5, 96, 66, 34, 255);
            setPx(x0+lx, y0+TILE_PX-6, 96, 66, 34, 255);
        }
    }
    {
        paintSolid(T_CHEST_SIDE, 138, 98, 54, 14, 337);
        int x0, y0; tileCell(T_CHEST_SIDE, x0, y0);
        for (int b = 0; b < 2; b++)                   // metal band (2px) + latch
        for (int lx = 0; lx < TILE_PX; lx++)
            setPx(x0+lx, y0+TILE_PX/2+b, 90, 62, 32, 255);
        for (int dy = -1; dy <= 2; dy++)
        for (int dx = -1; dx <= 1; dx++)
            setPx(x0+TILE_PX/2+dx, y0+TILE_PX/2+dy, 230, 210, 120, 255);
    }
    {   // Torch: brown stick with a glowing tip (cutout background)
        clearTile(T_TORCH);
        int x0, y0; tileCell(T_TORCH, x0, y0);
        int cx = TILE_PX/2;
        for (int ly = TILE_PX-1; ly >= TILE_PX/2; ly--) {      // stick
            setPx(x0+cx,   y0+ly, 120, 84, 46, 255);
            setPx(x0+cx-1, y0+ly, 96,  66, 34, 255);
        }
        for (int ly = TILE_PX/2-3; ly < TILE_PX/2+1; ly++)     // flame
        for (int dx = -1; dx <= 1; dx++)
            setPx(x0+cx+dx, y0+ly, 255, 210, 90, 255);
        setPx(x0+cx, y0+TILE_PX/2-4, 255, 240, 160, 255);
    }
    {   // Obsidian: near-black with faint purple flecks
        paintSolid(T_OBSIDIAN, 26, 18, 38, 10, 351);
        int x0, y0; tileCell(T_OBSIDIAN, x0, y0);
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++)
            if (pnoise(lx, ly, 352u) > 0.86f) setPx(x0+lx, y0+ly, 82, 52, 120, 255);
    }
    {   // Farmland: dark tilled soil with horizontal groove lines
        paintSolid(T_FARMLAND, 96, 60, 28, 18, 361);
        int x0, y0; tileCell(T_FARMLAND, x0, y0);
        for (int ly = 0; ly < TILE_PX; ly += 4)     // furrow groove every 4 rows
        for (int b = 0; b < 2; b++)
        for (int lx = 0; lx < TILE_PX; lx++) {
            int d = (int)((pnoise(lx, ly + b, 363u) - 0.5f) * 12);
            if (ly + b < TILE_PX) setPx(x0+lx, y0+ly+b, 60+d, 36+d, 14+d, 255);
        }
        // Slightly moist tinge on alternate furrow ridges
        for (int ly = 2; ly < TILE_PX; ly += 8)
        for (int lx = 0; lx < TILE_PX; lx++) {
            int d = (int)((pnoise(lx, ly, 367u) - 0.5f) * 10);
            setPx(x0+lx, y0+ly, 72+d, 46+d, 20+d, 255);
        }
    }

    // ── Fluids & volcanic rock ───────────────────────────────────────────
    {   // Lava: dark crust islands over a molten glow
        paintSolid(T_LAVA, 236, 106, 20, 30, 371);
        int x0, y0; tileCell(T_LAVA, x0, y0);
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++) {
            const float n = pnoise(lx / 4, ly / 4, 373u);
            if (n > 0.72f)       setPx(x0+lx, y0+ly, 74, 36, 28, 255);  // crust
            else if (n < 0.16f)  setPx(x0+lx, y0+ly, 255, 214, 92, 255); // hot core
        }
    }
    {   // Basalt: dark columnar stripes
        paintSolid(T_BASALT, 44, 42, 48, 16, 379);
        int x0, y0; tileCell(T_BASALT, x0, y0);
        for (int lx = 0; lx < TILE_PX; lx++)
            if (lx % 7 == 0)
                for (int ly = 0; ly < TILE_PX; ly++)
                    setPx(x0+lx, y0+ly, 28, 27, 32, 255);
    }

    // ── Construction materials ───────────────────────────────────────────
    {   // Stone bricks: large cut blocks, dark mortar, light top bevel
        paintSolid(T_STONE_BRICKS, 138, 138, 142, 14, 401);
        int x0, y0; tileCell(T_STONE_BRICKS, x0, y0);
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++) {
            const int off = ((ly / 7) & 1) * 7;
            if (ly % 7 == 6 || (lx + off) % 14 == 13)
                setPx(x0+lx, y0+ly, 92, 92, 98, 255);          // mortar
            else if (ly % 7 == 0)
                setPx(x0+lx, y0+ly, 158, 158, 162, 255);       // bevel
        }
    }
    {   // Red bricks with pale mortar
        paintSolid(T_BRICKS, 158, 74, 56, 18, 407);
        int x0, y0; tileCell(T_BRICKS, x0, y0);
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++) {
            const int off = ((ly / 5) & 1) * 5;
            if (ly % 5 == 4 || (lx + off) % 10 == 9)
                setPx(x0+lx, y0+ly, 198, 188, 178, 255);       // mortar
        }
    }
    // Marble: bright polished stone with faint grey veins
    paintSolid(T_MARBLE, 228, 226, 222, 6, 411);
    streaks(T_MARBLE, 198, 198, 204, 413u);

    {   // Lamps: dark wooden frame; ON has a warm glowing core
        auto lampTile = [&](uint16_t tile, int R, int G, int B, bool on, uint32_t seed) {
            paintSolid(tile, R, G, B, on ? 10 : 16, seed);
            int x0, y0; tileCell(tile, x0, y0);
            for (int b = 0; b < 3; b++)
            for (int l = 0; l < TILE_PX; l++) {
                setPx(x0+l, y0+b, 70, 60, 46, 255);
                setPx(x0+l, y0+TILE_PX-1-b, 70, 60, 46, 255);
                setPx(x0+b, y0+l, 70, 60, 46, 255);
                setPx(x0+TILE_PX-1-b, y0+l, 70, 60, 46, 255);
            }
            if (on)
                for (int ly = 9; ly < TILE_PX-9; ly++)
                for (int lx = 9; lx < TILE_PX-9; lx++)
                    setPx(x0+lx, y0+ly, 255, 244, 190, 255);
        };
        lampTile(T_LAMP_ON,  250, 214, 120, true,  421);
        lampTile(T_LAMP_OFF, 116, 108,  96, false, 423);
    }
    {   // Light switch (breaker panel): metal box, lever, state light
        auto switchTile = [&](uint16_t tile, bool on, uint32_t seed) {
            paintSolid(tile, 152, 154, 158, 10, seed);
            int x0, y0; tileCell(tile, x0, y0);
            for (int b = 0; b < 2; b++)
            for (int l = 0; l < TILE_PX; l++) {
                setPx(x0+l, y0+b, 96, 98, 104, 255);
                setPx(x0+l, y0+TILE_PX-1-b, 96, 98, 104, 255);
                setPx(x0+b, y0+l, 96, 98, 104, 255);
                setPx(x0+TILE_PX-1-b, y0+l, 96, 98, 104, 255);
            }
            for (int ly = 8; ly < 21; ly++)                // lever slot
            for (int lx = 12; lx < 16; lx++)
                setPx(x0+lx, y0+ly, 62, 64, 70, 255);
            const int hy = on ? 9 : 16;                    // lever handle
            for (int dy = 0; dy < 4; dy++)
            for (int dx = 0; dx < 6; dx++)
                setPx(x0+11+dx, y0+hy+dy, 210, 208, 200, 255);
            for (int dy = 0; dy < 3; dy++)                 // indicator light
            for (int dx = 0; dx < 3; dx++)
                setPx(x0+TILE_PX-8+dx, y0+5+dy,
                      on ? 80 : 200, on ? 200 : 60, on ? 90 : 50, 255);
        };
        switchTile(T_SWITCH_ON,  true,  427);
        switchTile(T_SWITCH_OFF, false, 429);
    }
    {   // Window pane: translucent glass, wooden frame + centre mullion
        int x0, y0; tileCell(T_WINDOW, x0, y0);
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++) {
            const int d = (int)((pnoise(lx, ly, 431u) - 0.5f) * 8);
            setPx(x0+lx, y0+ly, 205+d, 228+d, 240+d, 150);
        }
        for (int d = 4; d < 12; d++)                       // diagonal sheen
            setPx(x0+d, y0+15-d, 240, 250, 254, 190);
        auto frame = [&](int lx, int ly) { setPx(x0+lx, y0+ly, 150, 112, 66, 255); };
        for (int b = 0; b < 3; b++)
        for (int l = 0; l < TILE_PX; l++) {
            frame(l, b); frame(l, TILE_PX-1-b); frame(b, l); frame(TILE_PX-1-b, l);
        }
        for (int l = 0; l < TILE_PX; l++) {                // mullion cross
            frame(l, TILE_PX/2-1); frame(l, TILE_PX/2);
            frame(TILE_PX/2-1, l); frame(TILE_PX/2, l);
        }
    }
    {   // Iron bars: cutout vertical bars + rails
        clearTile(T_IRON_BARS);
        int x0, y0; tileCell(T_IRON_BARS, x0, y0);
        for (int lx = 2; lx < TILE_PX-1; lx += 6)
        for (int b = 0; b < 2; b++)
        for (int ly = 0; ly < TILE_PX; ly++) {
            const int d = (int)((pnoise(lx, ly, 437u) - 0.5f) * 14);
            setPx(x0+lx+b, y0+ly, 92+d, 96+d, 104+d, 255);
        }
        for (int lx = 0; lx < TILE_PX; lx++)
        for (int b = 0; b < 2; b++) {
            setPx(x0+lx, y0+b, 82, 86, 94, 255);
            setPx(x0+lx, y0+TILE_PX-1-b, 82, 86, 94, 255);
        }
    }
    {   // Wooden door: vertical planks + dark frame; window on top half,
        // golden handle on the bottom half. (Door UVs are not flipped.)
        auto doorBase = [&](uint16_t tile, uint32_t seed) {
            paintSolid(tile, 166, 126, 76, 12, seed);
            int x0, y0; tileCell(tile, x0, y0);
            for (int lx = 0; lx < TILE_PX; lx++)
                if (lx % 9 == 0)
                    for (int ly = 0; ly < TILE_PX; ly++)
                        setPx(x0+lx, y0+ly, 128, 94, 54, 255);
            for (int b = 0; b < 2; b++)
            for (int l = 0; l < TILE_PX; l++) {
                setPx(x0+l, y0+b, 118, 86, 48, 255);
                setPx(x0+l, y0+TILE_PX-1-b, 118, 86, 48, 255);
                setPx(x0+b, y0+l, 118, 86, 48, 255);
                setPx(x0+TILE_PX-1-b, y0+l, 118, 86, 48, 255);
            }
        };
        doorBase(T_DOOR_TOP, 441);
        {
            int x0, y0; tileCell(T_DOOR_TOP, x0, y0);
            for (int ly = 6; ly < 16; ly++)
            for (int lx = 7; lx < TILE_PX-7; lx++) {
                const bool mull = lx == TILE_PX/2 || lx == TILE_PX/2-1 ||
                                  ly == 10 || ly == 11;
                if (mull) setPx(x0+lx, y0+ly, 118, 86, 48, 255);
                else      setPx(x0+lx, y0+ly, 208, 230, 240, 255);
            }
        }
        doorBase(T_DOOR_BOT, 443);
        {
            int x0, y0; tileCell(T_DOOR_BOT, x0, y0);
            for (int dy = 0; dy < 3; dy++)
            for (int dx = 0; dx < 3; dx++)
                setPx(x0+TILE_PX-8+dx, y0+6+dy, 226, 208, 120, 255);
        }
    }
    {   // Bed top: white pillow band + red blanket with a fold line
        paintSolid(T_BED_TOP, 178, 52, 52, 12, 451);
        int x0, y0; tileCell(T_BED_TOP, x0, y0);
        for (int ly = 1; ly < 9; ly++)
        for (int lx = 2; lx < TILE_PX-2; lx++) {
            const int d = (int)((pnoise(lx, ly, 453u) - 0.5f) * 10);
            setPx(x0+lx, y0+ly, 236+d, 236+d, 240+d, 255);
        }
        for (int lx = 0; lx < TILE_PX; lx++) {
            setPx(x0+lx, y0+11, 146, 40, 40, 255);
            setPx(x0+lx, y0+12, 146, 40, 40, 255);
        }
        for (int ly = 0; ly < TILE_PX; ly++) {             // wooden rim
            setPx(x0, y0+ly, 110, 78, 44, 255);
            setPx(x0+TILE_PX-1, y0+ly, 110, 78, 44, 255);
        }
    }
    {   // Bed side: only the lower half of the tile is visible on the slab —
        // paint blanket / mattress / wood base into rows 14..27.
        paintSolid(T_BED_SIDE, 120, 84, 46, 14, 457);
        int x0, y0; tileCell(T_BED_SIDE, x0, y0);
        for (int ly = 14; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++) {
            const int d = (int)((pnoise(lx, ly, 459u) - 0.5f) * 10);
            if (ly < 18)      setPx(x0+lx, y0+ly, 178+d, 52+d, 52+d, 255);   // blanket
            else if (ly < 22) setPx(x0+lx, y0+ly, 236+d, 236+d, 240+d, 255); // mattress
            else              setPx(x0+lx, y0+ly, 116+d, 82+d, 46+d, 255);   // wood base
        }
    }
    {   // Sink top: ceramic with round basin, drain and a small faucet
        paintSolid(T_SINK_TOP, 226, 230, 234, 8, 461);
        int x0, y0; tileCell(T_SINK_TOP, x0, y0);
        const int c = TILE_PX/2;
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++) {
            const int dx = lx - c, dy = ly - c - 2;
            const int r2 = dx*dx + dy*dy;
            if (r2 <= 81)
                setPx(x0+lx, y0+ly, r2 <= 49 ? 178 : 204, r2 <= 49 ? 186 : 210,
                      r2 <= 49 ? 194 : 216, 255);
        }
        for (int dy = -1; dy <= 1; dy++)                   // drain
        for (int dx = -1; dx <= 1; dx++)
            setPx(x0+c+dx, y0+c+2+dy, 70, 74, 80, 255);
        for (int ly = 2; ly < 7; ly++) {                   // faucet stub
            setPx(x0+c-1, y0+ly, 172, 176, 182, 255);
            setPx(x0+c,   y0+ly, 186, 190, 196, 255);
        }
    }
    {   // Sink side: ceramic top band over a wooden cabinet with handles
        paintSolid(T_SINK_SIDE, 214, 218, 222, 8, 467);
        int x0, y0; tileCell(T_SINK_SIDE, x0, y0);
        for (int ly = 8; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++) {
            const int d = (int)((pnoise(lx, ly, 469u) - 0.5f) * 10);
            setPx(x0+lx, y0+ly, 150+d, 116+d, 74+d, 255);
        }
        for (int ly = 8; ly < TILE_PX; ly++) {             // cabinet split
            setPx(x0+TILE_PX/2-1, y0+ly, 116, 88, 54, 255);
            setPx(x0+TILE_PX/2,   y0+ly, 116, 88, 54, 255);
        }
        for (int dy = 0; dy < 3; dy++) {                   // door handles
            setPx(x0+TILE_PX/2-5, y0+14+dy, 220, 204, 130, 255);
            setPx(x0+TILE_PX/2+4, y0+14+dy, 220, 204, 130, 255);
        }
    }
    {   // Potted plant (cross billboard): terracotta pot + leafy sprigs
        clearTile(T_PLANT_POT);
        int x0, y0; tileCell(T_PLANT_POT, x0, y0);
        const int cx = TILE_PX/2;
        for (int ly = 18; ly < TILE_PX; ly++) {            // pot body
            const int hw = 7 - (ly - 18) / 4;
            for (int dx = -hw; dx <= hw; dx++) {
                const int d = (int)((pnoise(dx, ly, 471u) - 0.5f) * 16);
                setPx(x0+cx+dx, y0+ly, 168+d, 92+d, 58+d, 255);
            }
        }
        for (int dx = -8; dx <= 8; dx++)                   // pot rim
            setPx(x0+cx+dx, y0+17, 190, 108, 70, 255);
        for (int ly = 8; ly < 18; ly++)                    // stem
            setPx(x0+cx, y0+ly, 70, 120, 54, 255);
        for (int i = 0; i < 26; i++) {                     // leaves
            const int lx = cx + (int)((pnoise(i, 1, 473u) - 0.5f) * 14);
            const int ly = 4 + (int)(pnoise(1, i, 477u) * 12);
            setPx(x0+lx, y0+ly, 58 + (i%3)*14, 140 + (i%5)*10, 52, 255);
            setPx(x0+lx, y0+ly+1, 48, 118, 44, 255);
        }
    }

    // Season-dependent tiles start at the summer keyframe.
    paintSeasonTiles(SN::SEASON_TINTS[1]);
}

static void padTiles() {
    for (int tile = 0; tile < (int)T_COUNT; tile++) {
        int x0, y0;
        tileCell((uint16_t)tile, x0, y0);
        for (int y = 0; y < TILE_STRIDE; y++)
        for (int x = 0; x < TILE_STRIDE; x++) {
            if (x < TILE_PX && y < TILE_PX) continue;
            const int sx = x < TILE_PX ? x : TILE_PX - 1;
            const int sy = y < TILE_PX ? y : TILE_PX - 1;
            const int src = ((y0 + sy) * ATLAS_PX + (x0 + sx)) * 4;
            const int dst = ((y0 + y) * ATLAS_PX + (x0 + x)) * 4;
            g_buf[dst + 0] = g_buf[src + 0];
            g_buf[dst + 1] = g_buf[src + 1];
            g_buf[dst + 2] = g_buf[src + 2];
            g_buf[dst + 3] = g_buf[src + 3];
        }
    }
}

void TextureAtlas::init() {
    g_buf.assign((size_t)ATLAS_PX * ATLAS_PX * 4, 0);
    buildTiles();
    padTiles();

    pglGenTextures(1, &tex);
    pglBindTexture(GL_TEXTURE_2D, tex);
    pglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_PX, ATLAS_PX, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, g_buf.data());
    // Pixel-art look: no smoothing, clamp to avoid edge bleed
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, pglGenerateMipmap ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
    if (pglGenerateMipmap) pglGenerateMipmap(GL_TEXTURE_2D);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    pglBindTexture(GL_TEXTURE_2D, 0);

    // g_buf stays resident (256 KB): the seasons system repaints the
    // seasonal tiles in place and re-uploads as the year drifts.

    SDL_Log("[Atlas] %dx%d px, %d tiles, %ld KB uploaded",
            ATLAS_PX, ATLAS_PX, (int)T_COUNT, memoryBytes() / 1024);
}

void TextureAtlas::applySeason(const SN::SeasonTint& t) {
    if (!tex || g_buf.empty()) return;
    paintSeasonTiles(t);
    padTiles();

    pglBindTexture(GL_TEXTURE_2D, tex);
    pglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_PX, ATLAS_PX, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, g_buf.data());
    if (pglGenerateMipmap) pglGenerateMipmap(GL_TEXTURE_2D);
    pglBindTexture(GL_TEXTURE_2D, 0);
}

void TextureAtlas::setAnisotropic(float requested) {
    anisotropicApplied_ = 1.0f;
    if (!tex || !pglTexParameterf || !pglGetFloatv || requested <= 1.0f) return;
    float maxAniso = 1.0f;
    pglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    if (maxAniso <= 1.0f) return;
    anisotropicApplied_ = requested < maxAniso ? requested : maxAniso;
    pglBindTexture(GL_TEXTURE_2D, tex);
    pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropicApplied_);
    pglBindTexture(GL_TEXTURE_2D, 0);
}

void TextureAtlas::bind(int unit) const {
    pglActiveTexture(GL_TEXTURE0 + unit);
    pglBindTexture(GL_TEXTURE_2D, tex);
}

void TextureAtlas::cleanup() {
    if (tex) { pglDeleteTextures(1, &tex); tex = 0; }
}
