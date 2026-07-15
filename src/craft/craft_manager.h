#pragma once
// craft_manager.h — Central crafting orchestrator.
// Holds the player inventory, active station, recipe selection, and craft timer.
// All state is integrated here so save/load and UI have a single source of truth.
#include "inventory.h"
#include "recipe_database.h"
#include "craft_validator.h"
#include "craft_result.h"
#include "furnace_manager.h"
#include <cstring>
#include "SDL.h"

namespace Craft {

// ── CraftManager ─────────────────────────────────────────────────────────────
class CraftManager {
public:
    static constexpr int MAX_FILTERED = RECIPE_COUNT;

    CraftManager() { setStation(Station::Hand); }

    // ── Station ───────────────────────────────────────────────────────────────
    void setStation(Station s) {
        station_ = s;
        // Rebuild filtered recipe list
        filteredCount_ = RecipeDatabase::filterByStation(s, filteredIdx_, MAX_FILTERED);
        // Clamp selection
        if (selectedIdx_ >= filteredCount_) selectedIdx_ = 0;
    }

    Station activeStation() const { return station_; }

    const char* stationName() const {
        switch (station_) {
            case Station::Hand:      return "Hand";
            case Station::Workbench: return "Workbench";
            case Station::Furnace:   return "Furnace";
            default:                 return "?";
        }
    }

    // ── Recipe selection ──────────────────────────────────────────────────────
    void nextRecipe() {
        if (filteredCount_ == 0) return;
        selectedIdx_ = (selectedIdx_ + 1) % filteredCount_;
    }

    void prevRecipe() {
        if (filteredCount_ == 0) return;
        selectedIdx_ = (selectedIdx_ + filteredCount_ - 1) % filteredCount_;
    }

    void selectRecipe(int idx) {
        if (idx >= 0 && idx < filteredCount_) selectedIdx_ = idx;
    }

    const Recipe* selectedRecipe() const {
        if (filteredCount_ == 0) return nullptr;
        return &RECIPES[filteredIdx_[selectedIdx_]];
    }

    int selectedIndex()      const { return selectedIdx_; }
    int filteredRecipeCount()const { return filteredCount_; }

    // Recipe in the filtered list by position
    const Recipe* filteredAt(int pos) const {
        if (pos < 0 || pos >= filteredCount_) return nullptr;
        return &RECIPES[filteredIdx_[pos]];
    }

    // ── Inventory ─────────────────────────────────────────────────────────────
    Inventory& inventory()             { return inv_; }
    const Inventory& inventory() const { return inv_; }

    // Convenience: add item from broken block
    void giveBlock(uint8_t blockType, int count = 1) {
        ItemID id = blockDrop(blockType);
        if (id == ItemID::None) return;
        int added = inv_.addItem(id, count);
        if (added > 0)
            SDL_Log("[Craft] +%d %s", added, itemInfo(id).name);
    }

    // ── Craft execution ───────────────────────────────────────────────────────
    // Validates + starts timer. Returns result.
    CraftResult startCraft() {
        const Recipe* r = selectedRecipe();
        if (!r) { CraftResult res; res.status = CraftStatus::InvalidRecipe; return res; }

        if (craftTimeLeft_ > 0.0f) {
            CraftResult res; res.status = CraftStatus::AlreadyCrafting; return res;
        }

        CraftResult res = CraftValidator::validate(*r, inv_, station_);
        if (!res.ok()) return res;

        // Consume ingredients immediately (anti-duplication: deduct before timer)
        for (int i = 0; i < r->ingredientCount; i++)
            inv_.removeItem(r->ingredients[i].item, r->ingredients[i].count);

        pendingResult_ = res;
        pendingResult_.result = r->result;
        pendingResult_.count  = r->resultCount;

        if (r->craftTimeSecs <= 0.0f) {
            // Instant craft
            completeCraft_();
        } else {
            craftTimeLeft_ = r->craftTimeSecs;
            craftDuration_ = r->craftTimeSecs;
        }

        res.status = CraftStatus::Ok;
        return res;
    }

    // Instant craft (no timer): validate + consume + add result in one call.
    CraftResult craftNow() {
        const Recipe* r = selectedRecipe();
        if (!r) { CraftResult res; res.status = CraftStatus::InvalidRecipe; return res; }
        if (craftTimeLeft_ > 0.0f) {
            CraftResult res; res.status = CraftStatus::AlreadyCrafting; return res;
        }
        CraftResult res = CraftValidator::validate(*r, inv_, station_);
        if (!res.ok()) return res;

        for (int i = 0; i < r->ingredientCount; i++)
            inv_.removeItem(r->ingredients[i].item, r->ingredients[i].count);

        int added = inv_.addItem(r->result, r->resultCount);
        if (added == 0) {
            // Inventory full — refund (best effort)
            for (int i = 0; i < r->ingredientCount; i++)
                inv_.addItem(r->ingredients[i].item, r->ingredients[i].count);
            res.status = CraftStatus::InventoryFull;
            return res;
        }

        SDL_Log("[Craft] Crafted: %dx %s", r->resultCount, itemInfo(r->result).name);
        lastResultMsg_[0] = 0;
        snprintf(lastResultMsg_, sizeof(lastResultMsg_),
                 "Crafted %dx %s!", r->resultCount, itemInfo(r->result).name);
        msgTimer_ = 3.0f;
        return res;
    }

    bool isCrafting()     const { return craftTimeLeft_ > 0.0f; }
    float craftProgress() const {
        if (craftDuration_ <= 0.0f) return 1.0f;
        return 1.0f - craftTimeLeft_ / craftDuration_;
    }

    // ── Per-frame update ──────────────────────────────────────────────────────
    void update(float dt) {
        furnace_.update(dt);

        if (craftTimeLeft_ > 0.0f) {
            craftTimeLeft_ -= dt;
            if (craftTimeLeft_ <= 0.0f) {
                craftTimeLeft_ = 0.0f;
                completeCraft_();
            }
        }
        if (msgTimer_ > 0.0f) msgTimer_ -= dt;
    }

    // ── Furnace ───────────────────────────────────────────────────────────────
    FurnaceManager& furnace()             { return furnace_; }
    const FurnaceManager& furnace() const { return furnace_; }

    // ── Can-craft queries ─────────────────────────────────────────────────────
    bool canCraft(int filteredPos) const {
        const Recipe* r = filteredAt(filteredPos);
        if (!r) return false;
        return CraftValidator::validate(*r, inv_, station_).ok();
    }

    bool canCraftSelected() const { return canCraft(selectedIdx_); }

    // ── Status message (shown briefly after crafting) ─────────────────────────
    const char* lastMessage() const { return msgTimer_ > 0.0f ? lastResultMsg_ : ""; }

    // ── Debug ─────────────────────────────────────────────────────────────────
    void debugLog() const {
        SDL_Log("[CraftMgr] station=%s selected=%d/%d crafting=%s",
                stationName(), selectedIdx_, filteredCount_,
                isCrafting() ? "YES" : "no");
        const Recipe* r = selectedRecipe();
        if (r) {
            SDL_Log("[CraftMgr] Recipe: %s → %dx %s (%.1fs)",
                    r->name, r->resultCount, itemInfo(r->result).name, r->craftTimeSecs);
            for (int i = 0; i < r->ingredientCount; i++) {
                const auto& ing = r->ingredients[i];
                SDL_Log("[CraftMgr]   need %dx %s  have %d",
                        ing.count, itemInfo(ing.item).name, inv_.countItem(ing.item));
            }
        }
    }

private:
    Inventory      inv_;
    Station        station_    = Station::Hand;
    int            selectedIdx_   = 0;
    int            filteredIdx_[MAX_FILTERED];
    int            filteredCount_ = 0;
    float          craftTimeLeft_ = 0.0f;
    float          craftDuration_ = 0.0f;
    CraftResult    pendingResult_;
    FurnaceManager furnace_;
    char           lastResultMsg_[64] = {};
    float          msgTimer_ = 0.0f;

    void completeCraft_() {
        int added = inv_.addItem(pendingResult_.result, pendingResult_.count);
        if (added > 0) {
            SDL_Log("[Craft] Done: %dx %s", pendingResult_.count,
                    itemInfo(pendingResult_.result).name);
            snprintf(lastResultMsg_, sizeof(lastResultMsg_),
                     "Crafted %dx %s!",
                     pendingResult_.count, itemInfo(pendingResult_.result).name);
            msgTimer_ = 3.0f;
        } else {
            SDL_Log("[Craft] Inventory full — output lost!");
            snprintf(lastResultMsg_, sizeof(lastResultMsg_), "Inventory full!");
            msgTimer_ = 3.0f;
        }
        pendingResult_ = CraftResult{};
    }
};

} // namespace Craft
