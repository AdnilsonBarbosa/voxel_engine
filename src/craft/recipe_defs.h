#pragma once
// recipe_defs.h — Recipe data structures.
// All data-driven: no crafting logic here, only layout.
#include "item_defs.h"

namespace Craft {

// ── Crafting stations ─────────────────────────────────────────────────────────
enum class Station : uint8_t {
    Hand      = 0,  // bare hands — planks, sticks, torches, workbench
    Workbench = 1,  // requires a workbench nearby
    Furnace   = 2,  // requires a furnace (smelting)
};

// ── One ingredient (item + required count) ────────────────────────────────────
struct Ingredient {
    ItemID   item  = ItemID::None;
    uint16_t count = 0;
};

// ── Recipe definition ─────────────────────────────────────────────────────────
// Max 4 distinct ingredient types per recipe (covers all planned recipes).
struct Recipe {
    uint16_t   id;
    const char* name;
    Ingredient  ingredients[4];
    uint8_t     ingredientCount;
    ItemID      result;
    uint16_t    resultCount;
    float       craftTimeSecs;
    Station     station;
};

} // namespace Craft
