#pragma once
// craft_validator.h — Pure validation: does the inventory satisfy a recipe?
// No side effects; the CraftManager calls this before consuming ingredients.
#include "recipe_defs.h"
#include "inventory.h"
#include "craft_result.h"

namespace Craft {

class CraftValidator {
public:
    // Validate whether 'recipe' can be crafted right now.
    // activeStation: station the player is currently at.
    static CraftResult validate(const Recipe& recipe,
                                const Inventory& inv,
                                Station activeStation)
    {
        CraftResult r;

        // Check station requirement
        if (recipe.station > activeStation) {
            r.status = CraftStatus::WrongStation;
            return r;
        }

        // Check each ingredient
        for (int i = 0; i < recipe.ingredientCount; i++) {
            const auto& ing = recipe.ingredients[i];
            if (ing.item == ItemID::None) continue;
            if (!inv.hasItem(ing.item, ing.count)) {
                r.status = CraftStatus::NoMaterials;
                return r;
            }
        }

        r.status   = CraftStatus::Ok;
        r.result   = recipe.result;
        r.count    = recipe.resultCount;
        r.timeSecs = recipe.craftTimeSecs;
        return r;
    }

    // Check how many times a recipe can be crafted in one batch.
    static int maxBatch(const Recipe& recipe, const Inventory& inv) {
        int batch = 255;
        for (int i = 0; i < recipe.ingredientCount; i++) {
            const auto& ing = recipe.ingredients[i];
            if (ing.item == ItemID::None || ing.count == 0) continue;
            int have = inv.countItem(ing.item);
            int can  = have / ing.count;
            if (can < batch) batch = can;
        }
        return batch;
    }
};

} // namespace Craft
