#pragma once
// craft_result.h — Result type for a crafting attempt.
#include "item_defs.h"

namespace Craft {

enum class CraftStatus : uint8_t {
    Ok             = 0,
    NoMaterials    = 1,  // missing one or more ingredients
    WrongStation   = 2,  // need workbench / furnace
    InventoryFull  = 3,  // can't receive output
    InvalidRecipe  = 4,  // recipe ID not found
    AlreadyCrafting= 5,  // timer still running
};

struct CraftResult {
    CraftStatus status   = CraftStatus::Ok;
    ItemID      result   = ItemID::None;
    uint16_t    count    = 0;
    float       timeSecs = 0.0f;

    bool ok() const { return status == CraftStatus::Ok; }

    const char* message() const {
        switch (status) {
            case CraftStatus::Ok:              return "Crafted!";
            case CraftStatus::NoMaterials:     return "Missing materials";
            case CraftStatus::WrongStation:    return "Need workbench/furnace";
            case CraftStatus::InventoryFull:   return "Inventory full";
            case CraftStatus::InvalidRecipe:   return "Unknown recipe";
            case CraftStatus::AlreadyCrafting: return "Already crafting...";
            default:                           return "";
        }
    }
};

} // namespace Craft
