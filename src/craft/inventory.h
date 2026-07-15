#pragma once
// inventory.h — Fixed 36-slot item inventory with stacking and tool durability.
// No heap allocation, no STL containers in hot path.
#include "item_defs.h"
#include <cstring>
#include "SDL.h"

namespace Craft {

// ── Single inventory slot ─────────────────────────────────────────────────────
struct ItemSlot {
    ItemID   item       = ItemID::None;
    uint16_t count      = 0;
    uint16_t durability = 0;  // current durability; 0 = unused (full for new tool)
};

// ── Player inventory: 36 slots (4 rows × 9 hotbar) ───────────────────────────
class Inventory {
public:
    static constexpr int HOTBAR_SIZE = 9;
    static constexpr int TOTAL_SLOTS = 36;  // rows 0-3, hotbar = row 0

    Inventory() { clear(); }

    // ── Add item ─────────────────────────────────────────────────────────────
    // Returns number of items actually added (may be less if inventory full).
    int addItem(ItemID id, int count = 1) {
        if (id == ItemID::None || count <= 0) return 0;
        const ItemInfo& info = itemInfo(id);
        if (info.maxStack == 0) return 0;
        int added = 0;
        int remaining = count;

        // Step 1: stack with existing slots (for stackable items)
        if (info.maxStack > 1) {
            for (int i = 0; i < TOTAL_SLOTS && remaining > 0; i++) {
                if (slots_[i].item == id && slots_[i].count < info.maxStack) {
                    int space = info.maxStack - slots_[i].count;
                    int take  = remaining < space ? remaining : space;
                    slots_[i].count += (uint16_t)take;
                    remaining -= take;
                    added += take;
                }
            }
        }

        // Step 2: fill empty slots
        for (int i = 0; i < TOTAL_SLOTS && remaining > 0; i++) {
            if (slots_[i].item == ItemID::None) {
                int take = (info.maxStack > 0 && remaining > info.maxStack)
                         ? info.maxStack : remaining;
                slots_[i].item  = id;
                slots_[i].count = (uint16_t)take;
                // New tool: start at full durability
                slots_[i].durability = info.maxDurability;
                remaining -= take;
                added += take;
            }
        }
        return added;
    }

    // Remove count units of item. Returns true if all removed successfully.
    bool removeItem(ItemID id, int count = 1) {
        if (!hasItem(id, count)) return false;
        int remaining = count;
        for (int i = 0; i < TOTAL_SLOTS && remaining > 0; i++) {
            if (slots_[i].item == id) {
                int take = (remaining < slots_[i].count) ? remaining : (int)slots_[i].count;
                slots_[i].count -= (uint16_t)take;
                if (slots_[i].count == 0) slots_[i].item = ItemID::None;
                remaining -= take;
            }
        }
        return true;
    }

    // Count total of a given item across all slots.
    int countItem(ItemID id) const {
        int total = 0;
        for (int i = 0; i < TOTAL_SLOTS; i++)
            if (slots_[i].item == id) total += slots_[i].count;
        return total;
    }

    bool hasItem(ItemID id, int count = 1) const {
        return countItem(id) >= count;
    }

    // ── Tool durability ───────────────────────────────────────────────────────
    // Apply damage to the selected tool. Returns true if tool broke.
    bool damageTool(int slotIdx, int dmg) {
        if (slotIdx < 0 || slotIdx >= TOTAL_SLOTS) return false;
        ItemSlot& s = slots_[slotIdx];
        if (itemInfo(s.item).kind != ItemKind::Tool) return false;
        if (s.durability <= (uint16_t)dmg) {
            s.item = ItemID::None; s.count = 0; s.durability = 0;
            SDL_Log("[Craft] Tool in slot %d broke!", slotIdx);
            return true;
        }
        s.durability -= (uint16_t)dmg;
        return false;
    }

    // ── Hotbar ────────────────────────────────────────────────────────────────
    int selectedSlot()    const { return selected_; }
    void setSelected(int s)     { selected_ = (s >= 0 && s < HOTBAR_SIZE) ? s : 0; }
    const ItemSlot& heldItem()  const { return slots_[selected_]; }

    // ── Slot access ───────────────────────────────────────────────────────────
    const ItemSlot& slot(int i) const { return slots_[(i>=0&&i<TOTAL_SLOTS)?i:0]; }
    ItemSlot&       slot(int i)       { return slots_[(i>=0&&i<TOTAL_SLOTS)?i:0]; }

    void clear() {
        for (int i = 0; i < TOTAL_SLOTS; i++)
            slots_[i] = ItemSlot{};
        selected_ = 0;
    }

    // How full is the inventory (0-1)?
    float fillFraction() const {
        int used = 0;
        for (int i = 0; i < TOTAL_SLOTS; i++)
            if (slots_[i].item != ItemID::None) used++;
        return (float)used / TOTAL_SLOTS;
    }

    // Debug dump
    void debugLog() const {
        for (int i = 0; i < TOTAL_SLOTS; i++) {
            if (slots_[i].item == ItemID::None) continue;
            const auto& info = itemInfo(slots_[i].item);
            if (info.maxDurability > 0)
                SDL_Log("[Inv] slot%02d: %s x%d  dur=%d/%d",
                    i, info.name, slots_[i].count,
                    slots_[i].durability, info.maxDurability);
            else
                SDL_Log("[Inv] slot%02d: %s x%d",
                    i, info.name, slots_[i].count);
        }
    }

private:
    ItemSlot slots_[TOTAL_SLOTS];
    int      selected_ = 0;
};

} // namespace Craft
