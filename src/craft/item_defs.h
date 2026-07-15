#pragma once
// item_defs.h — Item catalogue, block-drop mapping, tool stats.
// No engine dependencies other than block_types.h.
#include "../world/block_types.h"
#include <cstdint>

namespace Craft {

// ── Item catalogue ────────────────────────────────────────────────────────────
enum class ItemID : uint16_t {
    None        = 0,
    // Raw blocks (harvestable)
    Wood        = 1,
    Cobblestone = 2,
    Dirt        = 3,
    Sand        = 4,
    Gravel      = 5,
    // Ore drops
    Coal        = 6,
    IronOre     = 7,
    GoldOre     = 8,
    Diamond     = 9,
    Copper      = 10,
    Emerald     = 11,
    // Smelted ingots / processed
    IronIngot   = 12,
    GoldIngot   = 13,
    Charcoal    = 14,
    // Crafted materials
    Plank       = 15,
    Stick       = 16,
    Torch       = 17,
    // Craft stations
    Workbench   = 18,
    Furnace     = 19,
    // ── Tools ─────────────────────────────────────────────────────────────────
    WoodPickaxe  = 20, StonePickaxe = 21, IronPickaxe = 22,
    WoodAxe      = 23, StoneAxe     = 24, IronAxe     = 25,
    WoodShovel   = 26, StoneShovel  = 27, IronShovel  = 28,
    WoodHoe      = 29, StoneHoe     = 30, IronHoe     = 31,
    WoodSword    = 32, StoneSword   = 33, IronSword   = 34,
    COUNT        = 35
};

static constexpr int ITEM_COUNT = (int)ItemID::COUNT;

enum class ItemKind : uint8_t {
    None    = 0,
    Block   = 1,  // stackable, can be placed in world
    Ore     = 2,  // raw ore / gem drop
    Ingot   = 3,  // smelted output
    Craft   = 4,  // crafted material (planks, sticks, torches)
    Tool    = 5,  // has durability, stack 1
    Station = 6,  // workbench / furnace (placeable block)
};

struct ItemInfo {
    const char* name;
    ItemKind    kind;
    uint16_t    maxStack;      // 1 for tools
    uint16_t    maxDurability; // 0 for non-tools
    BlockType   placeAs;       // BLOCK_AIR = not placeable
};

// clang-format off
static constexpr ItemInfo ITEM_INFO[ITEM_COUNT] = {
// id              name              kind              maxStack dur    placeAs
/*  0 */ {"None",         ItemKind::None,    0,   0,   BLOCK_AIR},
/*  1 */ {"Wood",         ItemKind::Block,   64,  0,   BLOCK_WOOD},
/*  2 */ {"Cobblestone",  ItemKind::Block,   64,  0,   BLOCK_COBBLESTONE},
/*  3 */ {"Dirt",         ItemKind::Block,   64,  0,   BLOCK_DIRT},
/*  4 */ {"Sand",         ItemKind::Block,   64,  0,   BLOCK_SAND},
/*  5 */ {"Gravel",       ItemKind::Block,   64,  0,   BLOCK_GRAVEL},
/*  6 */ {"Coal",         ItemKind::Ore,     64,  0,   BLOCK_AIR},
/*  7 */ {"Iron Ore",     ItemKind::Ore,     64,  0,   BLOCK_AIR},
/*  8 */ {"Gold Ore",     ItemKind::Ore,     64,  0,   BLOCK_AIR},
/*  9 */ {"Diamond",      ItemKind::Ore,     64,  0,   BLOCK_AIR},
/* 10 */ {"Copper Ore",   ItemKind::Ore,     64,  0,   BLOCK_AIR},
/* 11 */ {"Emerald",      ItemKind::Ore,     64,  0,   BLOCK_AIR},
/* 12 */ {"Iron Ingot",   ItemKind::Ingot,   64,  0,   BLOCK_AIR},
/* 13 */ {"Gold Ingot",   ItemKind::Ingot,   64,  0,   BLOCK_AIR},
/* 14 */ {"Charcoal",     ItemKind::Ingot,   64,  0,   BLOCK_AIR},
/* 15 */ {"Plank",        ItemKind::Craft,   64,  0,   BLOCK_PLANKS},
/* 16 */ {"Stick",        ItemKind::Craft,   64,  0,   BLOCK_AIR},
/* 17 */ {"Torch",        ItemKind::Craft,   64,  0,   BLOCK_TORCH},
/* 18 */ {"Workbench",    ItemKind::Station, 1,   0,   BLOCK_AIR}, // future: BLOCK_WORKBENCH
/* 19 */ {"Furnace",      ItemKind::Station, 1,   0,   BLOCK_AIR}, // future: BLOCK_FURNACE
/* 20 */ {"Wood Pickaxe", ItemKind::Tool,    1,   59,  BLOCK_AIR},
/* 21 */ {"Stone Pickaxe",ItemKind::Tool,    1,   131, BLOCK_AIR},
/* 22 */ {"Iron Pickaxe", ItemKind::Tool,    1,   250, BLOCK_AIR},
/* 23 */ {"Wood Axe",     ItemKind::Tool,    1,   59,  BLOCK_AIR},
/* 24 */ {"Stone Axe",    ItemKind::Tool,    1,   131, BLOCK_AIR},
/* 25 */ {"Iron Axe",     ItemKind::Tool,    1,   250, BLOCK_AIR},
/* 26 */ {"Wood Shovel",  ItemKind::Tool,    1,   59,  BLOCK_AIR},
/* 27 */ {"Stone Shovel", ItemKind::Tool,    1,   131, BLOCK_AIR},
/* 28 */ {"Iron Shovel",  ItemKind::Tool,    1,   250, BLOCK_AIR},
/* 29 */ {"Wood Hoe",     ItemKind::Tool,    1,   59,  BLOCK_AIR},
/* 30 */ {"Stone Hoe",    ItemKind::Tool,    1,   131, BLOCK_AIR},
/* 31 */ {"Iron Hoe",     ItemKind::Tool,    1,   250, BLOCK_AIR},
/* 32 */ {"Wood Sword",   ItemKind::Tool,    1,   59,  BLOCK_AIR},
/* 33 */ {"Stone Sword",  ItemKind::Tool,    1,   131, BLOCK_AIR},
/* 34 */ {"Iron Sword",   ItemKind::Tool,    1,   250, BLOCK_AIR},
};
// clang-format on

inline const ItemInfo& itemInfo(ItemID id) {
    int i = (int)id;
    return ITEM_INFO[(i >= 0 && i < ITEM_COUNT) ? i : 0];
}

// ── Tool stats (breakSpeed m/s, damage, efficiency vs correct block) ──────────
struct ToolStats { float breakSpeed; int damage; float efficiency; };
static constexpr ToolStats TOOL_STATS[ITEM_COUNT] = {
    // Non-tools: zeroed
    {},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},
    // 20 WoodPickaxe  21 StonePickaxe  22 IronPickaxe
    {2.0f,4,1.5f},  {4.0f,5,2.5f},   {6.0f,6,4.0f},
    // 23 WoodAxe      24 StoneAxe      25 IronAxe
    {2.0f,7,1.5f},  {4.0f,9,2.5f},   {6.0f,11,4.0f},
    // 26 WoodShovel   27 StoneShovel   28 IronShovel
    {2.0f,2,1.5f},  {4.0f,3,2.5f},   {6.0f,4,4.0f},
    // 29 WoodHoe      30 StoneHoe      31 IronHoe
    {2.0f,1,1.5f},  {4.0f,1,2.5f},   {6.0f,1,4.0f},
    // 32 WoodSword    33 StoneSword    34 IronSword
    {1.5f,8,1.0f},  {1.5f,11,1.0f},  {1.5f,14,1.0f},
};

inline const ToolStats& toolStats(ItemID id) {
    int i = (int)id;
    return TOOL_STATS[(i >= 0 && i < ITEM_COUNT) ? i : 0];
}

// ── Block → item drop ─────────────────────────────────────────────────────────
inline ItemID blockDrop(uint8_t block) {
    switch (block) {
        case BLOCK_WOOD:        return ItemID::Wood;
        case BLOCK_STONE:       return ItemID::Cobblestone; // stone drops cobblestone
        case BLOCK_COBBLESTONE: return ItemID::Cobblestone;
        case BLOCK_DEEPSLATE:   return ItemID::Cobblestone;
        case BLOCK_DIRT:        return ItemID::Dirt;
        case BLOCK_GRASS:       return ItemID::Dirt;        // grass → dirt
        case BLOCK_SAND:        return ItemID::Sand;
        case BLOCK_GRAVEL:      return ItemID::Gravel;
        case BLOCK_COAL:        return ItemID::Coal;
        case BLOCK_IRON:        return ItemID::IronOre;
        case BLOCK_GOLD:        return ItemID::GoldOre;
        case BLOCK_DIAMOND:     return ItemID::Diamond;
        case BLOCK_COPPER:      return ItemID::Copper;
        case BLOCK_EMERALD:     return ItemID::Emerald;
        case BLOCK_PLANKS:      return ItemID::Plank;
        case BLOCK_TORCH:       return ItemID::Torch;
        default:                return ItemID::None;
    }
}

} // namespace Craft
