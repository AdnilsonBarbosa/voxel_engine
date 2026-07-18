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
    // Tree species (wood/leaf variants)
    BLOCK_WOOD_BIRCH, BLOCK_LEAVES_PINE, BLOCK_LEAVES_BIRCH, BLOCK_LEAVES_AUTUMN,
    // Fluids & volcanic rock
    BLOCK_LAVA, BLOCK_BASALT,
    // Hanging flora (mangrove lianas)
    BLOCK_VINE,
    // ── Construction update: house-building materials ───────────────────────
    // Decorative rocks
    BLOCK_STONE_BRICKS, BLOCK_BRICKS, BLOCK_MARBLE,
    // Lighting fixtures (lamp toggles by interaction; switch drives nearby lamps)
    BLOCK_LAMP_ON, BLOCK_LAMP_OFF, BLOCK_SWITCH_ON, BLOCK_SWITCH_OFF,
    // Thin panels (orientation inferred from neighbours at mesh time)
    BLOCK_WINDOW, BLOCK_IRON_BARS,
    // Doors: NS = panel spans X (thin in Z), WE = panel spans Z (thin in X).
    // Open variants swing the panel 90° and become passable. Keep contiguous.
    BLOCK_DOOR_NS, BLOCK_DOOR_WE, BLOCK_DOOR_NS_OPEN, BLOCK_DOOR_WE_OPEN,
    // Furniture
    BLOCK_BED, BLOCK_SINK,
    // Stairs: suffix = ascending direction. Keep each set of 4 contiguous.
    BLOCK_STAIR_WOOD_PX, BLOCK_STAIR_WOOD_NX, BLOCK_STAIR_WOOD_PZ, BLOCK_STAIR_WOOD_NZ,
    BLOCK_STAIR_STONE_PX, BLOCK_STAIR_STONE_NX, BLOCK_STAIR_STONE_PZ, BLOCK_STAIR_STONE_NZ,
    // Decorative plant (cross billboard in a terracotta pot)
    BLOCK_PLANT_POT,
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
    {0.82f,0.80f,0.75f}, // WOOD_BIRCH (pale bark)
    {0.13f,0.34f,0.16f}, // LEAVES_PINE (dark needles)
    {0.40f,0.62f,0.24f}, // LEAVES_BIRCH (fresh light green)
    {0.73f,0.42f,0.16f}, // LEAVES_AUTUMN (orange)
    {0.96f,0.42f,0.08f}, // LAVA (molten glow)
    {0.16f,0.15f,0.17f}, // BASALT (dark volcanic rock)
    {0.24f,0.46f,0.18f}, // VINE (hanging liana)
    {0.58f,0.58f,0.60f}, // STONE_BRICKS
    {0.62f,0.30f,0.22f}, // BRICKS
    {0.90f,0.88f,0.86f}, // MARBLE
    {0.98f,0.86f,0.55f}, // LAMP_ON
    {0.45f,0.42f,0.38f}, // LAMP_OFF
    {0.60f,0.62f,0.64f}, // SWITCH_ON
    {0.58f,0.58f,0.60f}, // SWITCH_OFF
    {0.80f,0.88f,0.92f}, // WINDOW
    {0.38f,0.40f,0.44f}, // IRON_BARS
    {0.55f,0.40f,0.22f}, // DOOR_NS
    {0.55f,0.40f,0.22f}, // DOOR_WE
    {0.55f,0.40f,0.22f}, // DOOR_NS_OPEN
    {0.55f,0.40f,0.22f}, // DOOR_WE_OPEN
    {0.78f,0.24f,0.24f}, // BED
    {0.90f,0.92f,0.94f}, // SINK
    {0.72f,0.58f,0.36f}, // STAIR_WOOD_PX
    {0.72f,0.58f,0.36f}, // STAIR_WOOD_NX
    {0.72f,0.58f,0.36f}, // STAIR_WOOD_PZ
    {0.72f,0.58f,0.36f}, // STAIR_WOOD_NZ
    {0.58f,0.58f,0.60f}, // STAIR_STONE_PX
    {0.58f,0.58f,0.60f}, // STAIR_STONE_NX
    {0.58f,0.58f,0.60f}, // STAIR_STONE_PZ
    {0.58f,0.58f,0.60f}, // STAIR_STONE_NZ
    {0.30f,0.55f,0.24f}, // PLANT_POT
};

// Face shading — simulates directional light without a lighting pass.
// Art pass: slightly wider spread so block forms read at a glance.
static constexpr float FACE_SHADE[6] = {
    1.00f, // +Y top
    0.46f, // -Y bottom
    0.78f, // +X right
    0.71f, // -X left
    0.87f, // +Z front
    0.64f, // -Z back
};

// ── Render category ─────────────────────────────────────────────────────────
enum RenderKind : uint8_t {
    RK_AIR,     // nothing
    RK_SOLID,   // full opaque cube (face-culled, gets AO)
    RK_CROSS,   // X-shaped billboard plant (never culled)
    RK_LIQUID,  // water — animated surface
    RK_SLAB,    // lower half block (bed)
    RK_PANE,    // thin panel, orientation from neighbours (window, iron bars)
    RK_STAIR,   // half slab + upper back half; direction encoded in block id
    RK_DOOR,    // thin edge panel, 2 blocks tall; open state = own block id
};

static inline RenderKind blockRenderKind(uint8_t b) {
    switch (b) {
        case BLOCK_AIR:   return RK_AIR;
        case BLOCK_WATER: return RK_LIQUID;
        case BLOCK_LAVA:  return RK_LIQUID;
        case BLOCK_TALL_GRASS:
        case BLOCK_FLOWER_RED:
        case BLOCK_FLOWER_YELLOW:
        case BLOCK_MUSHROOM_RED:
        case BLOCK_MUSHROOM_BROWN:
        case BLOCK_BUSH:
        case BLOCK_VINE:
        case BLOCK_PLANT_POT:
        case BLOCK_TORCH: return RK_CROSS;
        case BLOCK_BED:   return RK_SLAB;
        case BLOCK_WINDOW:
        case BLOCK_IRON_BARS: return RK_PANE;
        case BLOCK_DOOR_NS: case BLOCK_DOOR_WE:
        case BLOCK_DOOR_NS_OPEN: case BLOCK_DOOR_WE_OPEN: return RK_DOOR;
        case BLOCK_STAIR_WOOD_PX: case BLOCK_STAIR_WOOD_NX:
        case BLOCK_STAIR_WOOD_PZ: case BLOCK_STAIR_WOOD_NZ:
        case BLOCK_STAIR_STONE_PX: case BLOCK_STAIR_STONE_NX:
        case BLOCK_STAIR_STONE_PZ: case BLOCK_STAIR_STONE_NZ: return RK_STAIR;
        default:          return RK_SOLID;
    }
}

// Only full opaque cubes occlude neighbor faces and cast ambient occlusion.
static inline bool blockIsOpaque(uint8_t b) {
    return blockRenderKind(b) == RK_SOLID;
}

// Physics solidity — partial shapes still block movement; open doors, plants
// and fluids do not. (Opaque = rendering concept, solid = collision concept.)
static inline bool blockIsSolid(uint8_t b) {
    switch (blockRenderKind(b)) {
        case RK_SOLID: case RK_SLAB: case RK_PANE: case RK_STAIR: return true;
        case RK_DOOR:  return b == BLOCK_DOOR_NS || b == BLOCK_DOOR_WE; // closed only
        default:       return false;
    }
}

// ── Door helpers ────────────────────────────────────────────────────────────
static inline bool blockIsDoor(uint8_t b) {
    return b >= BLOCK_DOOR_NS && b <= BLOCK_DOOR_WE_OPEN;
}
static inline uint8_t doorToggled(uint8_t b) {
    switch (b) {
        case BLOCK_DOOR_NS:      return BLOCK_DOOR_NS_OPEN;
        case BLOCK_DOOR_NS_OPEN: return BLOCK_DOOR_NS;
        case BLOCK_DOOR_WE:      return BLOCK_DOOR_WE_OPEN;
        case BLOCK_DOOR_WE_OPEN: return BLOCK_DOOR_WE;
        default:                 return b;
    }
}
