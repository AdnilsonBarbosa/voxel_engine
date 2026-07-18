#pragma once
#include "block_types.h"
#include "texture_atlas.h"
#include <cstdint>

// BlockMaterial — data model for every block: which atlas tiles it shows on
// each face, plus gameplay/visual properties. One flat table indexed by
// BlockType, so there is exactly one material per block (no duplicates).
namespace Materials {

using Atlas::TileId;

enum Category : uint8_t { CAT_AIR, CAT_TERRAIN, CAT_STONE, CAT_ORE,
                          CAT_WOOD, CAT_FOLIAGE, CAT_LIQUID, CAT_MISC };
enum SoundMat : uint8_t { SND_NONE, SND_GRASS, SND_DIRT, SND_STONE, SND_SAND,
                          SND_GRAVEL, SND_WOOD, SND_GLASS, SND_WATER };

// A face's texture: base tile + how many consecutive variant tiles follow it.
struct FaceTex { uint16_t tile; uint8_t variants; };

struct BlockMaterial {
    const char* name;
    uint8_t     id;
    FaceTex     top, side, bottom;
    float       hardness;      // relative mining time
    float       resistance;    // blast resistance
    bool        transparent;   // alpha cutout / doesn't fully occlude light
    uint8_t     lightEmit;     // 0..15 emitted light
    uint8_t     sound;         // SoundMat
    bool        breakable;     // can the player destroy it
    bool        receivesLight; // participates in the lighting pass (future)
    uint8_t     category;      // Category
};

// Helper macros to keep the table readable.
#define FT(t, v)  FaceTex{ (uint16_t)Atlas::t, (uint8_t)(v) }
#define SAME(t,v) FT(t,v), FT(t,v), FT(t,v)

static const BlockMaterial MATERIALS[BLOCK_COUNT] = {
//   name            id                 top                 side                bottom              hard  resist trans emit sound       brk  lit  category
    {"Air",          BLOCK_AIR,         FT(T_STONE0,1),     FT(T_STONE0,1),     FT(T_STONE0,1),     0.0f, 0.0f,  true,  0, SND_NONE,   false,false,CAT_AIR},
    {"Grass",        BLOCK_GRASS,       FT(T_GRASS_TOP0,3), FT(T_GRASS_SIDE,1), FT(T_DIRT0,3),      0.6f, 0.6f,  false, 0, SND_GRASS,  true, true, CAT_TERRAIN},
    {"Dirt",         BLOCK_DIRT,        SAME(T_DIRT0,3),                                            0.5f, 0.5f,  false, 0, SND_DIRT,   true, true, CAT_TERRAIN},
    {"Stone",        BLOCK_STONE,       SAME(T_STONE0,3),                                           1.5f, 6.0f,  false, 0, SND_STONE,  true, true, CAT_STONE},
    {"Sand",         BLOCK_SAND,        SAME(T_SAND0,2),                                            0.5f, 0.5f,  false, 0, SND_SAND,   true, true, CAT_TERRAIN},
    {"Gravel",       BLOCK_GRAVEL,      SAME(T_GRAVEL0,2),                                          0.6f, 0.6f,  false, 0, SND_GRAVEL, true, true, CAT_TERRAIN},
    {"Bedrock",      BLOCK_BEDROCK,     SAME(T_BEDROCK,1),                                          9.9f, 3600.f, false, 0, SND_STONE,  false,true, CAT_STONE},
    {"Coal Ore",     BLOCK_COAL,        SAME(T_COAL,1),                                             3.0f, 3.0f,  false, 0, SND_STONE,  true, true, CAT_ORE},
    {"Iron Ore",     BLOCK_IRON,        SAME(T_IRON,1),                                             3.0f, 3.0f,  false, 0, SND_STONE,  true, true, CAT_ORE},
    {"Gold Ore",     BLOCK_GOLD,        SAME(T_GOLD,1),                                             3.0f, 3.0f,  false, 0, SND_STONE,  true, true, CAT_ORE},
    {"Diamond Ore",  BLOCK_DIAMOND,     SAME(T_DIAMOND,1),                                          3.0f, 3.0f,  false, 0, SND_STONE,  true, true, CAT_ORE},
    {"Snow",         BLOCK_SNOW,        SAME(T_SNOW,1),                                             0.2f, 0.2f,  false, 0, SND_GRASS,  true, true, CAT_TERRAIN},
    {"Wood",         BLOCK_WOOD,        FT(T_WOOD_TOP,1),   FT(T_WOOD_SIDE,1),  FT(T_WOOD_TOP,1),   2.0f, 2.0f,  false, 0, SND_WOOD,   true, true, CAT_WOOD},
    {"Leaves",       BLOCK_LEAVES,      SAME(T_LEAVES,3),                                           0.2f, 0.2f,  true,  0, SND_GRASS,  true, true, CAT_FOLIAGE},
    {"Water",        BLOCK_WATER,       SAME(T_WATER,1),                                            0.0f, 100.f, true,  0, SND_WATER,  false,true, CAT_LIQUID},
    {"Tall Grass",   BLOCK_TALL_GRASS,  SAME(T_TALL_GRASS,1),                                       0.0f, 0.0f,  true,  0, SND_GRASS,  true, true, CAT_FOLIAGE},
    {"Red Flower",   BLOCK_FLOWER_RED,  SAME(T_FLOWER_RED,1),                                       0.0f, 0.0f,  true,  0, SND_GRASS,  true, true, CAT_FOLIAGE},
    {"Yellow Flower",BLOCK_FLOWER_YELLOW,SAME(T_FLOWER_YELLOW,1),                                   0.0f, 0.0f,  true,  0, SND_GRASS,  true, true, CAT_FOLIAGE},
    {"Red Mushroom", BLOCK_MUSHROOM_RED,SAME(T_MUSH_RED,1),                                         0.0f, 0.0f,  true,  0, SND_GRASS,  true, true, CAT_FOLIAGE},
    {"Brown Mushroom",BLOCK_MUSHROOM_BROWN,SAME(T_MUSH_BROWN,1),                                    0.0f, 0.0f,  true,  0, SND_GRASS,  true, true, CAT_FOLIAGE},
    {"Bush",         BLOCK_BUSH,        SAME(T_BUSH,1),                                             0.2f, 0.2f,  true,  0, SND_GRASS,  true, true, CAT_FOLIAGE},
    {"Rock",         BLOCK_ROCK,        SAME(T_ROCK,1),                                             1.0f, 1.0f,  false, 0, SND_STONE,  true, true, CAT_MISC},
    {"Deepslate",    BLOCK_DEEPSLATE,   SAME(T_DEEPSLATE0,2),                                        3.0f, 9.0f,  false, 0, SND_STONE,  true, true, CAT_STONE},
    {"Copper Ore",   BLOCK_COPPER,      SAME(T_COPPER,1),                                           3.0f, 3.0f,  false, 0, SND_STONE,  true, true, CAT_ORE},
    {"Emerald Ore",  BLOCK_EMERALD,     SAME(T_EMERALD,1),                                          3.0f, 3.0f,  false, 0, SND_STONE,  true, true, CAT_ORE},
    {"Cobblestone",  BLOCK_COBBLESTONE, SAME(T_COBBLE,1),                                           2.0f, 6.0f,  false, 0, SND_STONE,  true, true, CAT_STONE},
    {"Ice",          BLOCK_ICE,         SAME(T_ICE,1),                                              0.5f, 0.5f,  true,  0, SND_GLASS,  true, true, CAT_MISC},
    {"Clay",         BLOCK_CLAY,        SAME(T_CLAY,1),                                             0.6f, 0.6f,  false, 0, SND_DIRT,   true, true, CAT_MISC},
    {"Planks",       BLOCK_PLANKS,      SAME(T_PLANKS,1),                                           2.0f, 3.0f,  false, 0, SND_WOOD,   true, true, CAT_WOOD},
    {"Glass",        BLOCK_GLASS,       SAME(T_GLASS,1),                                            0.3f, 0.3f,  true,  0, SND_GLASS,  true, true, CAT_MISC},
    {"Chest",        BLOCK_CHEST,       FT(T_CHEST_TOP,1), FT(T_CHEST_SIDE,1), FT(T_CHEST_TOP,1),   2.5f, 2.5f,  false, 0, SND_WOOD,   true, true, CAT_MISC},
    {"Torch",        BLOCK_TORCH,       SAME(T_TORCH,1),                                            0.0f, 0.0f,  true, 14, SND_WOOD,   true, false,CAT_MISC},
    {"Obsidian",     BLOCK_OBSIDIAN,    SAME(T_OBSIDIAN,1),                                         50.f, 1200.f,false, 0, SND_STONE,  true, true, CAT_STONE},
    {"Farmland",     BLOCK_FARMLAND,    SAME(T_FARMLAND,1),                                         0.6f,   0.6f,false, 0, SND_DIRT,   true, true, CAT_TERRAIN},
    {"Birch Wood",   BLOCK_WOOD_BIRCH,  FT(T_WOOD_BIRCH_TOP,1),FT(T_WOOD_BIRCH_SIDE,1),FT(T_WOOD_BIRCH_TOP,1), 2.0f, 2.0f, false, 0, SND_WOOD, true, true, CAT_WOOD},
    {"Pine Leaves",  BLOCK_LEAVES_PINE, SAME(T_LEAVES_DARK,1),                                      0.2f, 0.2f,  true,  0, SND_GRASS,  true, true, CAT_FOLIAGE},
    {"Birch Leaves", BLOCK_LEAVES_BIRCH,SAME(T_LEAVES_BIRCH,1),                                     0.2f, 0.2f,  true,  0, SND_GRASS,  true, true, CAT_FOLIAGE},
    {"Autumn Leaves",BLOCK_LEAVES_AUTUMN,SAME(T_LEAVES_AUTUMN,1),                                   0.2f, 0.2f,  true,  0, SND_GRASS,  true, true, CAT_FOLIAGE},
    {"Lava",         BLOCK_LAVA,        SAME(T_LAVA,1),                                             0.0f, 100.f, false,12, SND_NONE,   false,true, CAT_LIQUID},
    {"Basalt",       BLOCK_BASALT,      SAME(T_BASALT,1),                                           2.5f, 8.0f,  false, 0, SND_STONE,  true, true, CAT_STONE},
    {"Vines",        BLOCK_VINE,        SAME(T_VINE,1),                                             0.0f, 0.0f,  true,  0, SND_GRASS,  true, true, CAT_FOLIAGE},
    {"Stone Bricks", BLOCK_STONE_BRICKS,SAME(T_STONE_BRICKS,1),                                     2.0f, 6.0f,  false, 0, SND_STONE,  true, true, CAT_STONE},
    {"Bricks",       BLOCK_BRICKS,      SAME(T_BRICKS,1),                                           2.0f, 6.0f,  false, 0, SND_STONE,  true, true, CAT_STONE},
    {"Marble",       BLOCK_MARBLE,      SAME(T_MARBLE,1),                                           2.0f, 6.0f,  false, 0, SND_STONE,  true, true, CAT_STONE},
    {"Lamp",         BLOCK_LAMP_ON,     SAME(T_LAMP_ON,1),                                          0.5f, 0.5f,  false,14, SND_GLASS,  true, false,CAT_MISC},
    {"Lamp (Off)",   BLOCK_LAMP_OFF,    SAME(T_LAMP_OFF,1),                                         0.5f, 0.5f,  false, 0, SND_GLASS,  true, true, CAT_MISC},
    {"Switch (On)",  BLOCK_SWITCH_ON,   SAME(T_SWITCH_ON,1),                                        0.5f, 0.5f,  false, 0, SND_STONE,  true, true, CAT_MISC},
    {"Switch (Off)", BLOCK_SWITCH_OFF,  SAME(T_SWITCH_OFF,1),                                       0.5f, 0.5f,  false, 0, SND_STONE,  true, true, CAT_MISC},
    {"Window",       BLOCK_WINDOW,      SAME(T_WINDOW,1),                                           0.3f, 0.3f,  true,  0, SND_GLASS,  true, true, CAT_MISC},
    {"Iron Bars",    BLOCK_IRON_BARS,   SAME(T_IRON_BARS,1),                                        1.5f, 3.0f,  true,  0, SND_STONE,  true, true, CAT_MISC},
    {"Wooden Door",  BLOCK_DOOR_NS,     SAME(T_DOOR_BOT,1),                                         1.5f, 2.0f,  true,  0, SND_WOOD,   true, true, CAT_WOOD},
    {"Wooden Door",  BLOCK_DOOR_WE,     SAME(T_DOOR_BOT,1),                                         1.5f, 2.0f,  true,  0, SND_WOOD,   true, true, CAT_WOOD},
    {"Wooden Door",  BLOCK_DOOR_NS_OPEN,SAME(T_DOOR_BOT,1),                                         1.5f, 2.0f,  true,  0, SND_WOOD,   true, true, CAT_WOOD},
    {"Wooden Door",  BLOCK_DOOR_WE_OPEN,SAME(T_DOOR_BOT,1),                                         1.5f, 2.0f,  true,  0, SND_WOOD,   true, true, CAT_WOOD},
    {"Bed",          BLOCK_BED,         FT(T_BED_TOP,1),    FT(T_BED_SIDE,1),   FT(T_PLANKS,1),     0.8f, 0.8f,  true,  0, SND_WOOD,   true, true, CAT_MISC},
    {"Sink",         BLOCK_SINK,        FT(T_SINK_TOP,1),   FT(T_SINK_SIDE,1),  FT(T_MARBLE,1),     1.5f, 2.0f,  false, 0, SND_STONE,  true, true, CAT_MISC},
    {"Wood Stairs",  BLOCK_STAIR_WOOD_PX, SAME(T_PLANKS,1),                                         2.0f, 3.0f,  true,  0, SND_WOOD,   true, true, CAT_WOOD},
    {"Wood Stairs",  BLOCK_STAIR_WOOD_NX, SAME(T_PLANKS,1),                                         2.0f, 3.0f,  true,  0, SND_WOOD,   true, true, CAT_WOOD},
    {"Wood Stairs",  BLOCK_STAIR_WOOD_PZ, SAME(T_PLANKS,1),                                         2.0f, 3.0f,  true,  0, SND_WOOD,   true, true, CAT_WOOD},
    {"Wood Stairs",  BLOCK_STAIR_WOOD_NZ, SAME(T_PLANKS,1),                                         2.0f, 3.0f,  true,  0, SND_WOOD,   true, true, CAT_WOOD},
    {"Stone Stairs", BLOCK_STAIR_STONE_PX,SAME(T_STONE_BRICKS,1),                                   2.0f, 6.0f,  true,  0, SND_STONE,  true, true, CAT_STONE},
    {"Stone Stairs", BLOCK_STAIR_STONE_NX,SAME(T_STONE_BRICKS,1),                                   2.0f, 6.0f,  true,  0, SND_STONE,  true, true, CAT_STONE},
    {"Stone Stairs", BLOCK_STAIR_STONE_PZ,SAME(T_STONE_BRICKS,1),                                   2.0f, 6.0f,  true,  0, SND_STONE,  true, true, CAT_STONE},
    {"Stone Stairs", BLOCK_STAIR_STONE_NZ,SAME(T_STONE_BRICKS,1),                                   2.0f, 6.0f,  true,  0, SND_STONE,  true, true, CAT_STONE},
    {"Potted Plant", BLOCK_PLANT_POT,   SAME(T_PLANT_POT,1),                                        0.2f, 0.2f,  true,  0, SND_GRASS,  true, true, CAT_FOLIAGE},
};

#undef FT
#undef SAME

inline const BlockMaterial& of(uint8_t block) { return MATERIALS[block]; }

// Tile for a given face index (0=+Y top,1=-Y bottom, else side) with a
// per-block variant chosen from the material's variant range.
inline uint16_t tileForFace(uint8_t block, int faceIndex, uint32_t variant) {
    const BlockMaterial& m = MATERIALS[block];
    const FaceTex& f = (faceIndex == 0) ? m.top
                     : (faceIndex == 1) ? m.bottom : m.side;
    return (uint16_t)(f.tile + (f.variants ? variant % f.variants : 0));
}

inline int materialCount() { return BLOCK_COUNT; }

} // namespace Materials
