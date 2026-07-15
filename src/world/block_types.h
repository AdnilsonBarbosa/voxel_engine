#pragma once
#include <cstdint>

// ── Sea level: terrain below this floods with water ─────────────────────────
static constexpr int WATER_LEVEL = 62;

enum BlockType : uint8_t {
    BLOCK_AIR = 0,
    BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE,
    BLOCK_SAND,  BLOCK_GRAVEL, BLOCK_BEDROCK,
    BLOCK_COAL,  BLOCK_IRON,   BLOCK_GOLD,   BLOCK_DIAMOND,
    BLOCK_SNOW,  BLOCK_WOOD,   BLOCK_LEAVES,
    BLOCK_WATER,
    BLOCK_TALL_GRASS, BLOCK_FLOWER_RED, BLOCK_FLOWER_YELLOW,
    BLOCK_MUSHROOM_RED, BLOCK_MUSHROOM_BROWN, BLOCK_BUSH,
    BLOCK_ROCK,
    // Underground layer + additional ores
    BLOCK_DEEPSLATE, BLOCK_COPPER, BLOCK_EMERALD,
    // Extra building blocks (materials defined; not placed by world gen yet)
    BLOCK_COBBLESTONE, BLOCK_ICE, BLOCK_CLAY,
    // Structure blocks
    BLOCK_PLANKS, BLOCK_GLASS, BLOCK_CHEST,
    BLOCK_TORCH, BLOCK_OBSIDIAN,
    // Agriculture
    BLOCK_FARMLAND,
    BLOCK_COUNT
};

struct BlockColor { float r, g, b; };

// RGB per block type (index matches BlockType)
static constexpr BlockColor BLOCK_COLORS[BLOCK_COUNT] = {
    {0,0,0},
    {0.35f,0.60f,0.20f}, // GRASS
    {0.53f,0.35f,0.18f}, // DIRT
    {0.50f,0.50f,0.50f}, // STONE
    {0.93f,0.87f,0.60f}, // SAND
    {0.47f,0.44f,0.40f}, // GRAVEL
    {0.15f,0.15f,0.15f}, // BEDROCK
    {0.28f,0.28f,0.28f}, // COAL
    {0.55f,0.47f,0.42f}, // IRON
    {0.80f,0.70f,0.20f}, // GOLD
    {0.20f,0.80f,0.90f}, // DIAMOND
    {0.95f,0.97f,1.00f}, // SNOW
    {0.42f,0.30f,0.16f}, // WOOD
    {0.18f,0.42f,0.14f}, // LEAVES
    {0.20f,0.42f,0.78f}, // WATER
    {0.32f,0.62f,0.20f}, // TALL_GRASS
    {0.88f,0.22f,0.22f}, // FLOWER_RED
    {0.95f,0.85f,0.22f}, // FLOWER_YELLOW
    {0.82f,0.16f,0.16f}, // MUSHROOM_RED
    {0.58f,0.42f,0.28f}, // MUSHROOM_BROWN
    {0.22f,0.46f,0.16f}, // BUSH
    {0.46f,0.46f,0.48f}, // ROCK
    {0.28f,0.28f,0.32f}, // DEEPSLATE
    {0.72f,0.45f,0.30f}, // COPPER (oxidised orange-brown)
    {0.15f,0.75f,0.45f}, // EMERALD
    {0.42f,0.42f,0.44f}, // COBBLESTONE
    {0.68f,0.85f,0.95f}, // ICE
    {0.60f,0.64f,0.72f}, // CLAY
    {0.72f,0.58f,0.36f}, // PLANKS
    {0.78f,0.88f,0.92f}, // GLASS
    {0.55f,0.40f,0.22f}, // CHEST
    {1.00f,0.72f,0.25f}, // TORCH
    {0.09f,0.06f,0.13f}, // OBSIDIAN
    {0.40f,0.24f,0.10f}, // FARMLAND (dark tilled soil)
};

// Face shading — simulates directional light without a lighting pass
static constexpr float FACE_SHADE[6] = {
    1.00f, // +Y top
    0.50f, // -Y bottom
    0.80f, // +X right
    0.75f, // -X left
    0.85f, // +Z front
    0.70f, // -Z back
};

// ── Render category ─────────────────────────────────────────────────────────
enum RenderKind : uint8_t {
    RK_AIR,     // nothing
    RK_SOLID,   // full opaque cube (face-culled, gets AO)
    RK_CROSS,   // X-shaped billboard plant (never culled)
    RK_LIQUID,  // water — animated surface
};

static inline RenderKind blockRenderKind(uint8_t b) {
    switch (b) {
        case BLOCK_AIR:   return RK_AIR;
        case BLOCK_WATER: return RK_LIQUID;
        case BLOCK_TALL_GRASS:
        case BLOCK_FLOWER_RED:
        case BLOCK_FLOWER_YELLOW:
        case BLOCK_MUSHROOM_RED:
        case BLOCK_MUSHROOM_BROWN:
        case BLOCK_BUSH:
        case BLOCK_TORCH: return RK_CROSS;
        default:          return RK_SOLID;
    }
}

// Only full opaque cubes occlude neighbor faces and cast ambient occlusion.
static inline bool blockIsOpaque(uint8_t b) {
    return blockRenderKind(b) == RK_SOLID;
}
