#pragma once
// craft_tests.h — Automated crafting system tests.
// Call runCraftTests() once at startup to verify the system.
#include "craft_manager.h"
#include "SDL.h"

namespace Craft {

inline bool runCraftTests() {
    SDL_Log("[CraftTest] === Running crafting system tests ===");
    int passed = 0, failed = 0;

    auto PASS = [&](const char* name) {
        SDL_Log("[CraftTest] PASS: %s", name);
        passed++;
    };
    auto FAIL = [&](const char* name, const char* reason) {
        SDL_Log("[CraftTest] FAIL: %s — %s", name, reason);
        failed++;
    };

    // ── Test 1: Craft planks from 1 wood ─────────────────────────────────────
    {
        CraftManager cm;
        cm.inventory().addItem(ItemID::Wood, 1);
        cm.setStation(Station::Hand);
        // Find the plank recipe
        for (int i = 0; i < cm.filteredRecipeCount(); i++) {
            const Recipe* r = cm.filteredAt(i);
            if (r && r->result == ItemID::Plank) { cm.selectRecipe(i); break; }
        }
        CraftResult res = cm.craftNow();
        if (res.ok() && cm.inventory().countItem(ItemID::Plank) == 4
                     && cm.inventory().countItem(ItemID::Wood) == 0)
            PASS("Planks: 1 Wood → 4 Planks");
        else
            FAIL("Planks", res.message());
    }

    // ── Test 2: Craft sticks from 2 planks ───────────────────────────────────
    {
        CraftManager cm;
        cm.inventory().addItem(ItemID::Plank, 2);
        cm.setStation(Station::Hand);
        for (int i = 0; i < cm.filteredRecipeCount(); i++) {
            const Recipe* r = cm.filteredAt(i);
            if (r && r->result == ItemID::Stick) { cm.selectRecipe(i); break; }
        }
        CraftResult res = cm.craftNow();
        if (res.ok() && cm.inventory().countItem(ItemID::Stick) == 4)
            PASS("Sticks: 2 Planks → 4 Sticks");
        else
            FAIL("Sticks", res.message());
    }

    // ── Test 3: Craft torches ─────────────────────────────────────────────────
    {
        CraftManager cm;
        cm.inventory().addItem(ItemID::Coal, 1);
        cm.inventory().addItem(ItemID::Stick, 1);
        cm.setStation(Station::Hand);
        for (int i = 0; i < cm.filteredRecipeCount(); i++) {
            const Recipe* r = cm.filteredAt(i);
            if (r && r->result == ItemID::Torch) { cm.selectRecipe(i); break; }
        }
        CraftResult res = cm.craftNow();
        if (res.ok() && cm.inventory().countItem(ItemID::Torch) == 4)
            PASS("Torches: Coal + Stick → 4 Torches");
        else
            FAIL("Torches", res.message());
    }

    // ── Test 4: Craft workbench ───────────────────────────────────────────────
    {
        CraftManager cm;
        cm.inventory().addItem(ItemID::Plank, 4);
        cm.setStation(Station::Hand);
        for (int i = 0; i < cm.filteredRecipeCount(); i++) {
            const Recipe* r = cm.filteredAt(i);
            if (r && r->result == ItemID::Workbench) { cm.selectRecipe(i); break; }
        }
        CraftResult res = cm.craftNow();
        if (res.ok() && cm.inventory().countItem(ItemID::Workbench) == 1)
            PASS("Workbench: 4 Planks → 1 Workbench");
        else
            FAIL("Workbench", res.message());
    }

    // ── Test 5: Craft wood pickaxe (needs workbench) ──────────────────────────
    {
        CraftManager cm;
        cm.inventory().addItem(ItemID::Plank, 3);
        cm.inventory().addItem(ItemID::Stick, 2);
        cm.setStation(Station::Workbench);
        for (int i = 0; i < cm.filteredRecipeCount(); i++) {
            const Recipe* r = cm.filteredAt(i);
            if (r && r->result == ItemID::WoodPickaxe) { cm.selectRecipe(i); break; }
        }
        CraftResult res = cm.craftNow();
        if (res.ok() && cm.inventory().countItem(ItemID::WoodPickaxe) == 1) {
            // Verify durability was set
            const ItemInfo& info = itemInfo(ItemID::WoodPickaxe);
            bool durOk = false;
            for (int i = 0; i < Inventory::TOTAL_SLOTS; i++) {
                const ItemSlot& s = cm.inventory().slot(i);
                if (s.item == ItemID::WoodPickaxe && s.durability == info.maxDurability)
                    durOk = true;
            }
            if (durOk) PASS("Wood Pickaxe: 3 Planks + 2 Sticks → Pickaxe (full dur)");
            else        FAIL("Wood Pickaxe", "durability not set correctly");
        } else {
            FAIL("Wood Pickaxe", res.message());
        }
    }

    // ── Test 6: Craft axe ─────────────────────────────────────────────────────
    {
        CraftManager cm;
        cm.inventory().addItem(ItemID::Cobblestone, 3);
        cm.inventory().addItem(ItemID::Stick, 2);
        cm.setStation(Station::Workbench);
        for (int i = 0; i < cm.filteredRecipeCount(); i++) {
            const Recipe* r = cm.filteredAt(i);
            if (r && r->result == ItemID::StoneAxe) { cm.selectRecipe(i); break; }
        }
        CraftResult res = cm.craftNow();
        if (res.ok() && cm.inventory().countItem(ItemID::StoneAxe) == 1)
            PASS("Stone Axe: 3 Cobblestone + 2 Sticks → Stone Axe");
        else
            FAIL("Stone Axe", res.message());
    }

    // ── Test 7: Fail without materials ───────────────────────────────────────
    {
        CraftManager cm;
        cm.setStation(Station::Hand);
        // Find plank recipe
        for (int i = 0; i < cm.filteredRecipeCount(); i++) {
            const Recipe* r = cm.filteredAt(i);
            if (r && r->result == ItemID::Plank) { cm.selectRecipe(i); break; }
        }
        CraftResult res = cm.craftNow();
        if (!res.ok() && res.status == CraftStatus::NoMaterials)
            PASS("Reject: no materials → NoMaterials");
        else
            FAIL("Reject no-materials", "should have returned NoMaterials");
    }

    // ── Test 8: Fail wrong station (pickaxe without workbench) ───────────────
    {
        CraftManager cm;
        cm.inventory().addItem(ItemID::Plank, 3);
        cm.inventory().addItem(ItemID::Stick, 2);
        cm.setStation(Station::Hand);  // <- wrong! pickaxe needs Workbench
        for (int i = 0; i < cm.filteredRecipeCount(); i++) {
            const Recipe* r = cm.filteredAt(i);
            if (r && r->result == ItemID::WoodPickaxe) { cm.selectRecipe(i); break; }
        }
        // WoodPickaxe is not in Hand-filtered list, so selectedRecipe() → nullptr
        const Recipe* sel = cm.selectedRecipe();
        bool correct = (sel == nullptr || sel->result != ItemID::WoodPickaxe);
        if (correct)
            PASS("Station gate: WoodPickaxe invisible at Hand station");
        else
            FAIL("Station gate", "Pickaxe should not appear in Hand recipe list");
    }

    // ── Test 9: Furnace smelt iron ────────────────────────────────────────────
    {
        CraftManager cm;
        cm.inventory().addItem(ItemID::IronOre, 2);
        cm.inventory().addItem(ItemID::Coal, 1);
        bool fuelOk  = cm.furnace().addFuel(cm.inventory(), ItemID::Coal, 1);
        bool inputOk = cm.furnace().addInput(cm.inventory(), ItemID::IronOre, 2);
        // Simulate 25 seconds of smelting (2 × 10s + a bit extra)
        for (int tick = 0; tick < 250; tick++)
            cm.furnace().update(0.1f);
        int taken = cm.furnace().takeOutput(cm.inventory());
        if (fuelOk && inputOk && cm.inventory().countItem(ItemID::IronIngot) == 2)
            PASS("Furnace: Coal + 2 IronOre → 2 IronIngot");
        else
            FAIL("Furnace", "expected 2 iron ingots in inventory");
    }

    // ── Test 10: Crafting does not duplicate ──────────────────────────────────
    {
        CraftManager cm;
        cm.inventory().addItem(ItemID::Wood, 1);
        cm.setStation(Station::Hand);
        for (int i = 0; i < cm.filteredRecipeCount(); i++) {
            const Recipe* r = cm.filteredAt(i);
            if (r && r->result == ItemID::Plank) { cm.selectRecipe(i); break; }
        }
        cm.craftNow();
        cm.craftNow(); // no wood left, should fail
        if (cm.inventory().countItem(ItemID::Plank) == 4
         && cm.inventory().countItem(ItemID::Wood) == 0)
            PASS("Anti-duplication: second craft fails gracefully");
        else
            FAIL("Anti-duplication", "duplicate detected");
    }

    // ── Test 11: Furnace rejects non-smeltable input ──────────────────────────
    {
        CraftManager cm;
        cm.inventory().addItem(ItemID::Diamond, 1);
        bool bad = cm.furnace().addInput(cm.inventory(), ItemID::Diamond, 1);
        if (!bad && cm.inventory().countItem(ItemID::Diamond) == 1)
            PASS("Furnace: rejects non-smeltable Diamond");
        else
            FAIL("Furnace reject", "should not accept Diamond as input");
    }

    SDL_Log("[CraftTest] === Results: %d passed, %d failed ===", passed, failed);
    return failed == 0;
}

} // namespace Craft
