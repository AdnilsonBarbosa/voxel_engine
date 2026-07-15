#pragma once
// furnace_manager.h — Fuel-driven smelting queue.
// Fuel burns → input ore smelts → output lands in player inventory.
#include "inventory.h"
#include "recipe_defs.h"
#include <cmath>
#include "SDL.h"

namespace Craft {

// ── Fuel values (seconds of burn time) ───────────────────────────────────────
inline float fuelValue(ItemID id) {
    switch (id) {
        case ItemID::Coal:     return 80.0f;
        case ItemID::Charcoal: return 80.0f;
        case ItemID::Wood:     return 15.0f;
        case ItemID::Plank:    return  5.0f;
        case ItemID::Stick:    return  2.0f;
        default:               return  0.0f;
    }
}

// ── Smelt input → output ──────────────────────────────────────────────────────
inline ItemID smeltOutput(ItemID input) {
    switch (input) {
        case ItemID::IronOre:  return ItemID::IronIngot;
        case ItemID::GoldOre:  return ItemID::GoldIngot;
        case ItemID::Wood:     return ItemID::Charcoal;
        default:               return ItemID::None;
    }
}

static constexpr float SMELT_TIME = 10.0f; // seconds per item

// ── Furnace internal state ────────────────────────────────────────────────────
struct FurnaceState {
    ItemID   fuelItem     = ItemID::None;
    uint16_t fuelCount    = 0;
    ItemID   inputItem    = ItemID::None;
    uint16_t inputCount   = 0;
    ItemID   outputItem   = ItemID::None;
    uint16_t outputCount  = 0;
    float    fuelTimeLeft = 0.0f;  // seconds remaining in current fuel unit
    float    smeltAccum   = 0.0f;  // progress toward SMELT_TIME for current item
};

class FurnaceManager {
public:
    // Add fuel to the furnace (pulled from player inventory).
    bool addFuel(Inventory& inv, ItemID fuel, int n = 1) {
        float val = fuelValue(fuel);
        if (val <= 0.0f || !inv.hasItem(fuel, n)) return false;
        if (s_.fuelItem != ItemID::None && s_.fuelItem != fuel) return false;
        inv.removeItem(fuel, n);
        s_.fuelItem  = fuel;
        s_.fuelCount = (uint16_t)(s_.fuelCount + n);
        SDL_Log("[Furnace] Added fuel: %s x%d (%.0fs each)",
                itemInfo(fuel).name, n, val);
        return true;
    }

    // Add input ore/wood to smelt (pulled from player inventory).
    bool addInput(Inventory& inv, ItemID input, int n = 1) {
        if (smeltOutput(input) == ItemID::None) return false;
        if (!inv.hasItem(input, n)) return false;
        if (s_.inputItem != ItemID::None && s_.inputItem != input) return false;
        inv.removeItem(input, n);
        s_.inputItem  = input;
        s_.inputCount = (uint16_t)(s_.inputCount + n);
        SDL_Log("[Furnace] Added input: %s x%d", itemInfo(input).name, n);
        return true;
    }

    // Take finished output into player inventory.
    int takeOutput(Inventory& inv) {
        if (s_.outputItem == ItemID::None || s_.outputCount == 0) return 0;
        int added = inv.addItem(s_.outputItem, s_.outputCount);
        s_.outputCount -= (uint16_t)added;
        if (s_.outputCount == 0) s_.outputItem = ItemID::None;
        return added;
    }

    // Per-frame update (call even when furnace screen not open).
    void update(float dt) {
        if (s_.inputItem == ItemID::None || s_.inputCount == 0) return;
        if (smeltOutput(s_.inputItem) == ItemID::None) return;

        // Consume fuel to keep burning
        if (s_.fuelTimeLeft <= 0.0f) {
            if (s_.fuelCount > 0 && s_.fuelItem != ItemID::None) {
                s_.fuelTimeLeft += fuelValue(s_.fuelItem);
                s_.fuelCount--;
                if (s_.fuelCount == 0) s_.fuelItem = ItemID::None;
            } else {
                return; // no fuel, stop
            }
        }

        s_.fuelTimeLeft -= dt;
        s_.smeltAccum   += dt;

        if (s_.smeltAccum >= SMELT_TIME) {
            s_.smeltAccum -= SMELT_TIME;
            ItemID out = smeltOutput(s_.inputItem);
            s_.outputItem  = out;
            s_.outputCount++;
            s_.inputCount--;
            if (s_.inputCount == 0) s_.inputItem = ItemID::None;
            SDL_Log("[Furnace] Smelted → %s", itemInfo(out).name);
        }
    }

    bool  isActive()     const { return s_.fuelTimeLeft > 0.0f && s_.inputCount > 0; }
    bool  hasOutput()    const { return s_.outputCount > 0; }
    float smeltFraction()const { return s_.smeltAccum / SMELT_TIME; }
    float fuelFraction() const {
        float maxFuel = (s_.fuelItem != ItemID::None)
                      ? fuelValue(s_.fuelItem) : 80.0f;
        return (maxFuel > 0.0f) ? s_.fuelTimeLeft / maxFuel : 0.0f;
    }

    const FurnaceState& state() const { return s_; }
    FurnaceState&       state()       { return s_; }

    void reset() { s_ = FurnaceState{}; }

private:
    FurnaceState s_;
};

} // namespace Craft
