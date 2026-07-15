#pragma once
// recipe_database.h — Static registry of all recipes.
// Add new recipes here; the rest of the system auto-discovers them.
#include "recipe_defs.h"

namespace Craft {

// ── Master recipe table ───────────────────────────────────────────────────────
// Layout: {id, name, {{item,count},...}, ingCount, result, resultCount, timeSecs, station}
// clang-format off
static constexpr Recipe RECIPES[] = {
// ── Hand crafting ─────────────────────────────────────────────────────────────
{  1, "Plank",
   {{ItemID::Wood,1}}, 1,
   ItemID::Plank, 4,   0.5f, Station::Hand },

{  2, "Stick",
   {{ItemID::Plank,2}}, 1,
   ItemID::Stick, 4,   0.5f, Station::Hand },

{  3, "Torch",
   {{ItemID::Coal,1},{ItemID::Stick,1}}, 2,
   ItemID::Torch, 4,   1.0f, Station::Hand },

{  4, "Workbench",
   {{ItemID::Plank,4}}, 1,
   ItemID::Workbench, 1, 2.0f, Station::Hand },

// ── Workbench ─────────────────────────────────────────────────────────────────
{  5, "Furnace",
   {{ItemID::Cobblestone,8}}, 1,
   ItemID::Furnace, 1,  3.0f, Station::Workbench },

// Pickaxes
{  6, "Wood Pickaxe",
   {{ItemID::Plank,3},{ItemID::Stick,2}}, 2,
   ItemID::WoodPickaxe, 1, 3.0f, Station::Workbench },

{  7, "Stone Pickaxe",
   {{ItemID::Cobblestone,3},{ItemID::Stick,2}}, 2,
   ItemID::StonePickaxe, 1, 4.0f, Station::Workbench },

{  8, "Iron Pickaxe",
   {{ItemID::IronIngot,3},{ItemID::Stick,2}}, 2,
   ItemID::IronPickaxe, 1, 5.0f, Station::Workbench },

// Axes
{  9, "Wood Axe",
   {{ItemID::Plank,3},{ItemID::Stick,2}}, 2,
   ItemID::WoodAxe, 1,   3.0f, Station::Workbench },

{ 10, "Stone Axe",
   {{ItemID::Cobblestone,3},{ItemID::Stick,2}}, 2,
   ItemID::StoneAxe, 1,  4.0f, Station::Workbench },

{ 11, "Iron Axe",
   {{ItemID::IronIngot,3},{ItemID::Stick,2}}, 2,
   ItemID::IronAxe, 1,   5.0f, Station::Workbench },

// Shovels
{ 12, "Wood Shovel",
   {{ItemID::Plank,1},{ItemID::Stick,2}}, 2,
   ItemID::WoodShovel, 1, 3.0f, Station::Workbench },

{ 13, "Stone Shovel",
   {{ItemID::Cobblestone,1},{ItemID::Stick,2}}, 2,
   ItemID::StoneShovel, 1, 4.0f, Station::Workbench },

{ 14, "Iron Shovel",
   {{ItemID::IronIngot,1},{ItemID::Stick,2}}, 2,
   ItemID::IronShovel, 1, 5.0f, Station::Workbench },

// Hoes
{ 15, "Wood Hoe",
   {{ItemID::Plank,2},{ItemID::Stick,2}}, 2,
   ItemID::WoodHoe, 1,   3.0f, Station::Workbench },

{ 16, "Stone Hoe",
   {{ItemID::Cobblestone,2},{ItemID::Stick,2}}, 2,
   ItemID::StoneHoe, 1,  4.0f, Station::Workbench },

{ 17, "Iron Hoe",
   {{ItemID::IronIngot,2},{ItemID::Stick,2}}, 2,
   ItemID::IronHoe, 1,   5.0f, Station::Workbench },

// Swords
{ 18, "Wood Sword",
   {{ItemID::Plank,2},{ItemID::Stick,1}}, 2,
   ItemID::WoodSword, 1,  3.0f, Station::Workbench },

{ 19, "Stone Sword",
   {{ItemID::Cobblestone,2},{ItemID::Stick,1}}, 2,
   ItemID::StoneSword, 1, 4.0f, Station::Workbench },

{ 20, "Iron Sword",
   {{ItemID::IronIngot,2},{ItemID::Stick,1}}, 2,
   ItemID::IronSword, 1,  5.0f, Station::Workbench },

// ── Furnace ───────────────────────────────────────────────────────────────────
{ 21, "Smelt Iron",
   {{ItemID::IronOre,1}}, 1,
   ItemID::IronIngot, 1, 10.0f, Station::Furnace },

{ 22, "Smelt Gold",
   {{ItemID::GoldOre,1}}, 1,
   ItemID::GoldIngot, 1, 10.0f, Station::Furnace },

{ 23, "Charcoal",
   {{ItemID::Wood,1}}, 1,
   ItemID::Charcoal, 1,  10.0f, Station::Furnace },
};
// clang-format on

static constexpr int RECIPE_COUNT = (int)(sizeof(RECIPES) / sizeof(RECIPES[0]));

// ── RecipeDatabase — lookup helpers ───────────────────────────────────────────
class RecipeDatabase {
public:
    // Find recipe by its ID. Returns nullptr if not found.
    static const Recipe* byId(uint16_t id) {
        for (int i = 0; i < RECIPE_COUNT; i++)
            if (RECIPES[i].id == id) return &RECIPES[i];
        return nullptr;
    }

    // Fill outIds[] with recipe indices craftable at this station (or any
    // station <= station). Returns count written.
    static int filterByStation(Station station, int outIds[], int maxOut) {
        int n = 0;
        for (int i = 0; i < RECIPE_COUNT && n < maxOut; i++)
            if (RECIPES[i].station <= station)  // Hand ⊂ Workbench subset
                outIds[n++] = i;
        return n;
    }

    // Filter: only recipes craftable at exactly this station.
    static int filterExact(Station station, int outIds[], int maxOut) {
        int n = 0;
        for (int i = 0; i < RECIPE_COUNT && n < maxOut; i++)
            if (RECIPES[i].station == station)
                outIds[n++] = i;
        return n;
    }
};

} // namespace Craft
