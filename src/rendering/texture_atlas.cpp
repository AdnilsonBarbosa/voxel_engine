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
    x0 = (tile % ATLAS_COLS) * TILE_PX;
    y0 = (tile / ATLAS_COLS) * TILE_PX;
}

// Flat speckled block (dirt/stone/sand/…). `jit` = brightness jitter amplitude.
static void paintSolid(uint16_t tile, int R, int G, int B, int jit, uint32_t seed) {
    int x0, y0; tileCell(tile, x0, y0);
    for (int ly = 0; ly < TILE_PX; ly++)
    for (int lx = 0; lx < TILE_PX; lx++) {
        int d = (int)((pnoise(lx, ly, seed) - 0.5f) * jit);
        setPx(x0+lx, y0+ly, R+d, G+d, B+d, 255);
    }
}

// Stamp a few coloured ore blobs over an already-painted base tile.
static void paintOre(uint16_t tile, int oreR, int oreG, int oreB, uint32_t seed) {
    int x0, y0; tileCell(tile, x0, y0);
    for (int b = 0; b < 6; b++) {
        int bx = 2 + (int)(pnoise(b, 0, seed) * (TILE_PX - 5));
        int by = 2 + (int)(pnoise(0, b, seed) * (TILE_PX - 5));
        for (int dy = 0; dy < 2 + (b & 1); dy++)
        for (int dx = 0; dx < 2 + (b & 1); dx++) {
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

static void buildTiles() {
    // ── Terrain / stone ──────────────────────────────────────────────────
    paintSolid(T_GRASS_TOP0, 90, 140, 55, 28, 11);
    paintSolid(T_GRASS_TOP1, 96, 150, 58, 30, 23);
    paintSolid(T_GRASS_TOP2, 84, 132, 50, 26, 37);
    paintSolid(T_DIRT0, 134, 96, 62, 26, 41);
    paintSolid(T_DIRT1, 128, 92, 58, 26, 53);
    paintSolid(T_DIRT2, 140, 100, 66, 26, 67);

    // Grass side = dirt with a jagged green top band
    {
        paintSolid(T_GRASS_SIDE, 134, 96, 62, 24, 71);
        int x0, y0; tileCell(T_GRASS_SIDE, x0, y0);
        for (int lx = 0; lx < TILE_PX; lx++) {
            int edge = 3 + (int)(pnoise(lx, 99, 5u) * 3);   // jagged grass fringe
            for (int ly = 0; ly < edge; ly++) {
                int d = (int)((pnoise(lx, ly, 12u) - 0.5f) * 28);
                setPx(x0+lx, y0+ly, 90+d, 140+d, 55+d, 255);
            }
        }
    }

    paintSolid(T_STONE0, 122, 122, 126, 22, 101);
    paintSolid(T_STONE1, 116, 116, 120, 22, 103);
    paintSolid(T_STONE2, 128, 128, 132, 22, 107);

    // Cobblestone = stone with a blocky cell pattern
    {
        paintSolid(T_COBBLE, 120, 120, 124, 30, 131);
        int x0, y0; tileCell(T_COBBLE, x0, y0);
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++)
            if ((lx % 8 == 0) || (ly % 8 == 0) || ((lx+ly) % 7 == 0))
                setPx(x0+lx, y0+ly, 70, 70, 74, 255);   // dark mortar lines
    }

    paintSolid(T_SAND0, 224, 208, 152, 20, 151);
    paintSolid(T_SAND1, 218, 202, 146, 20, 157);
    paintSolid(T_GRAVEL0, 120, 112, 104, 34, 161);
    paintSolid(T_GRAVEL1, 128, 118, 110, 34, 167);
    paintSolid(T_BEDROCK, 46, 46, 50, 44, 181);
    paintSolid(T_DEEPSLATE0, 66, 66, 78, 22, 191);
    paintSolid(T_DEEPSLATE1, 60, 60, 72, 22, 193);

    // ── Wood ─────────────────────────────────────────────────────────────
    {
        paintSolid(T_WOOD_SIDE, 120, 84, 46, 18, 201);
        int x0, y0; tileCell(T_WOOD_SIDE, x0, y0);
        for (int lx = 0; lx < TILE_PX; lx++)
            if (pnoise(lx, 7, 202u) > 0.72f)                // vertical grain lines
                for (int ly = 0; ly < TILE_PX; ly++)
                    setPx(x0+lx, y0+ly, 92, 62, 34, 255);
    }
    {
        paintSolid(T_WOOD_TOP, 150, 110, 66, 14, 211);
        int x0, y0; tileCell(T_WOOD_TOP, x0, y0);
        float c = (TILE_PX - 1) * 0.5f;
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++) {
            float r = sqrtf((lx-c)*(lx-c) + (ly-c)*(ly-c));
            if (((int)r) % 3 == 0) setPx(x0+lx, y0+ly, 118, 84, 48, 255); // rings
        }
    }

    // ── Foliage (cutout) ─────────────────────────────────────────────────
    {
        paintSolid(T_LEAVES, 54, 118, 44, 34, 221);
        int x0, y0; tileCell(T_LEAVES, x0, y0);
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++)
            if (pnoise(lx, ly, 222u) < 0.14f) setPx(x0+lx, y0+ly, 0, 0, 0, 0); // holes
    }

    paintSolid(T_SNOW, 236, 240, 248, 14, 231);
    paintSolid(T_ICE,  150, 194, 226, 12, 241);
    paintSolid(T_CLAY, 156, 162, 176, 16, 251);
    paintSolid(T_WATER, 46, 96, 176, 16, 261);

    // ── Ores (stone / deepslate base + coloured blobs) ───────────────────
    paintSolid(T_COAL,    120,120,126,18,101); paintOre(T_COAL,    38, 38, 42, 271);
    paintSolid(T_IRON,    120,120,126,18,101); paintOre(T_IRON,    198,150,110, 277);
    paintSolid(T_COPPER,  120,120,126,18,101); paintOre(T_COPPER,  196,110, 70, 281);
    paintSolid(T_GOLD,    120,120,126,18,101); paintOre(T_GOLD,    224,192, 70, 283);
    paintSolid(T_DIAMOND, 66,66,78,18,191);    paintOre(T_DIAMOND, 110,224,224, 287);
    paintSolid(T_EMERALD, 66,66,78,18,191);    paintOre(T_EMERALD, 60,200,110, 293);

    // ── Cross plants (cutout shapes on transparent) ──────────────────────
    auto blades = [&](uint16_t tile, int R, int G, int B) {
        clearTile(tile);
        int x0, y0; tileCell(tile, x0, y0);
        for (int lx = 0; lx < TILE_PX; lx++) {
            if (pnoise(lx, 3, tile) < 0.45f) continue;      // sparse blades
            int top = 3 + (int)(pnoise(lx, 8, tile) * 7);
            for (int ly = TILE_PX - 1; ly >= top; ly--) {
                int d = (int)((pnoise(lx, ly, tile) - 0.5f) * 24);
                setPx(x0+lx, y0+ly, R+d, G+d, B+d, 255);
            }
        }
    };
    blades(T_TALL_GRASS, 74, 138, 52);
    blades(T_BUSH,       58, 110, 44);

    auto flower = [&](uint16_t tile, int R, int G, int B) {
        clearTile(tile);
        int x0, y0; tileCell(tile, x0, y0);
        int cx = TILE_PX/2;
        for (int ly = TILE_PX-1; ly >= TILE_PX/2; ly--)     // green stem
            setPx(x0+cx, y0+ly, 60, 120, 46, 255);
        for (int dy = 2; dy < 6; dy++)                       // coloured head
        for (int dx = -2; dx <= 2; dx++)
            if (dx*dx + (dy-4)*(dy-4) <= 6)
                setPx(x0+cx+dx, y0+dy, R, G, B, 255);
    };
    flower(T_FLOWER_RED,    210, 60, 60);
    flower(T_FLOWER_YELLOW, 226, 200, 60);

    auto mushroom = [&](uint16_t tile, int R, int G, int B) {
        clearTile(tile);
        int x0, y0; tileCell(tile, x0, y0);
        int cx = TILE_PX/2;
        for (int ly = TILE_PX-1; ly >= TILE_PX/2 + 1; ly--)  // pale stem
            setPx(x0+cx, y0+ly, 214, 206, 190, 255);
        for (int dy = 3; dy <= 7; dy++)                       // cap
        for (int dx = -3; dx <= 3; dx++)
            if (dy - 3 >= abs(dx) - 1)
                setPx(x0+cx+dx, y0+dy, R, G, B, 255);
    };
    mushroom(T_MUSH_RED,   200, 50, 50);
    mushroom(T_MUSH_BROWN, 150, 110, 78);

    paintSolid(T_ROCK, 110, 110, 114, 26, 301);   // small solid rock

    // ── Structure blocks ─────────────────────────────────────────────────
    {
        paintSolid(T_PLANKS, 178, 140, 88, 16, 311);
        int x0, y0; tileCell(T_PLANKS, x0, y0);
        for (int ly = 0; ly < TILE_PX; ly++)
        for (int lx = 0; lx < TILE_PX; lx++)
            if (ly % 4 == 0 || (lx % 8 == (ly / 4) * 3 % 8))  // plank seams
                setPx(x0+lx, y0+ly, 138, 104, 62, 255);
    }
    {
        paintSolid(T_GLASS, 200, 224, 236, 6, 321);
        int x0, y0; tileCell(T_GLASS, x0, y0);
        for (int lx = 0; lx < TILE_PX; lx++) {        // pane frame
            setPx(x0+lx, y0+0, 150, 170, 182, 255);
            setPx(x0+lx, y0+TILE_PX-1, 150, 170, 182, 255);
            setPx(x0+0, y0+lx, 150, 170, 182, 255);
            setPx(x0+TILE_PX-1, y0+lx, 150, 170, 182, 255);
        }
    }
    {
        paintSolid(T_CHEST_TOP, 150, 108, 60, 14, 331);
        int x0, y0; tileCell(T_CHEST_TOP, x0, y0);
        for (int lx = 2; lx < TILE_PX-2; lx++) {      // lid rim
            setPx(x0+lx, y0+2, 96, 66, 34, 255);
            setPx(x0+lx, y0+TILE_PX-3, 96, 66, 34, 255);
        }
    }
    {
        paintSolid(T_CHEST_SIDE, 138, 98, 54, 14, 337);
        int x0, y0; tileCell(T_CHEST_SIDE, x0, y0);
        for (int lx = 0; lx < TILE_PX; lx++)          // metal band + latch
            setPx(x0+lx, y0+TILE_PX/2, 90, 62, 32, 255);
        setPx(x0+TILE_PX/2, y0+TILE_PX/2, 230, 210, 120, 255);
        setPx(x0+TILE_PX/2, y0+TILE_PX/2+1, 230, 210, 120, 255);
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
        for (int ly = 0; ly < TILE_PX; ly += 2)     // furrow groove every 2 rows
        for (int lx = 0; lx < TILE_PX; lx++) {
            int d = (int)((pnoise(lx, ly, 363u) - 0.5f) * 12);
            setPx(x0+lx, y0+ly, 60+d, 36+d, 14+d, 255);
        }
        // Slightly moist tinge on alternate furrow ridges
        for (int ly = 1; ly < TILE_PX; ly += 4)
        for (int lx = 0; lx < TILE_PX; lx++) {
            int d = (int)((pnoise(lx, ly, 367u) - 0.5f) * 10);
            setPx(x0+lx, y0+ly, 72+d, 46+d, 20+d, 255);
        }
    }
}

void TextureAtlas::init() {
    g_buf.assign((size_t)ATLAS_PX * ATLAS_PX * 4, 0);
    buildTiles();

    pglGenTextures(1, &tex);
    pglBindTexture(GL_TEXTURE_2D, tex);
    pglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_PX, ATLAS_PX, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, g_buf.data());
    // Pixel-art look: no smoothing, clamp to avoid edge bleed
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    pglBindTexture(GL_TEXTURE_2D, 0);

    std::vector<unsigned char>().swap(g_buf);   // free CPU copy

    SDL_Log("[Atlas] %dx%d px, %d tiles, %ld KB uploaded",
            ATLAS_PX, ATLAS_PX, (int)T_COUNT, memoryBytes() / 1024);
}

void TextureAtlas::bind(int unit) const {
    pglActiveTexture(GL_TEXTURE0 + unit);
    pglBindTexture(GL_TEXTURE_2D, tex);
}

void TextureAtlas::cleanup() {
    if (tex) { pglDeleteTextures(1, &tex); tex = 0; }
}
