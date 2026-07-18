#pragma once
// crafting_ui.h — Lightweight overlay UI using DebugOverlay text primitives.
// No heap allocation, no animation, mobile-safe.
// Open: C key.  Navigate: N/P keys.  Craft: E key.  Close: C or Escape.
#include "craft_manager.h"
#include "../rendering/debug_overlay.h"
#include "../ui/ui_theme.h"
#include <cstdio>
#include <cstring>

namespace Craft {

class CraftingUI {
public:
    bool isOpen() const { return open_; }
    void toggle() { open_ = !open_; dirty_ = true; }
    void close()  { open_ = false; }

    // ── Click on the Craft button (pixel coords) ─────────────────────────────
    // Returns true if the click was consumed.
    bool handleClick(int mx, int my, CraftManager& craft) {
        if (!open_) return false;
        if (hitTest_(mx, my, craftBtnX_, craftBtnY_, craftBtnW_, craftBtnH_)) {
            CraftResult r = craft.craftNow();
            lastMsg_[0] = 0;
            strncpy(lastMsg_, r.message(), sizeof(lastMsg_) - 1);
            dirty_ = true;
            return true;
        }
        // Recipe list: click a row to select it
        for (int i = 0; i < listRowCount_ && i < craft.filteredRecipeCount(); i++) {
            int ry = listY_ + i * ROW_H;
            if (hitTest_(mx, my, listX_, ry, listW_, ROW_H)) {
                craft.selectRecipe(i);
                dirty_ = true;
                return true;
            }
        }
        // Furnace take-output button
        if (hitTest_(mx, my, furnBtnX_, furnBtnY_, furnBtnW_, furnBtnH_)) {
            craft.furnace().takeOutput(craft.inventory());
            dirty_ = true;
            return true;
        }
        return true; // any click inside the panel is consumed
    }

    // ── Draw the entire crafting panel ────────────────────────────────────────
    void draw(DebugOverlay& ov, CraftManager& craft, int screenW, int screenH) {
        if (!open_) return;

        // Panel layout (anchored top-right)
        int panW = 420, panH = 480;
        int panX = screenW - panW - 10;
        int panY = 10;
        UI::panel(ov, panX, panY, panW, panH, 0.92f);

        int x = panX + 8, y = panY + 8;
        const int LINE = 14;

        // ── Title + station ───────────────────────────────────────────────────
        char buf[128];
        snprintf(buf, sizeof(buf), "CRAFTING  [%s]  (C=close N/P=nav E=craft)",
                 craft.stationName());
        ov.drawText(buf, x, y, 0x00ffcc); y += LINE + 2;

        // Station switch hint
        ov.drawText("Q=Hand  W=Workbench  F=Furnace", x, y, 0x446688); y += LINE + 4;

        // ── Inventory ─────────────────────────────────────────────────────────
        ov.drawText("INVENTORY:", x, y, 0xffcc44); y += LINE;
        {
            int col = 0;
            for (int i = 0; i < Inventory::TOTAL_SLOTS; i++) {
                const ItemSlot& s = craft.inventory().slot(i);
                if (s.item == ItemID::None) continue;
                const ItemInfo& info = itemInfo(s.item);
                if (info.maxDurability > 0)
                    snprintf(buf, sizeof(buf), "%s:%d(%d%%)",
                             info.name, s.count,
                             (int)(100.0f * s.durability / info.maxDurability));
                else
                    snprintf(buf, sizeof(buf), "%s:%d", info.name, s.count);
                ov.drawText(buf, x + col * 140, y, 0xaaaaff);
                col++;
                if (col >= 3) { col = 0; y += LINE; }
            }
            if (col > 0) y += LINE;
        }
        y += 4;

        // ── Recipe list (up to 10 visible) ────────────────────────────────────
        ov.drawText("RECIPES:", x, y, 0xffcc44); y += LINE;
        listX_ = x; listY_ = y; listW_ = panW - 16;
        int totalR = craft.filteredRecipeCount();
        int show   = totalR < 10 ? totalR : 10;
        listRowCount_ = show;

        int startIdx = craft.selectedIndex() - show / 2;
        if (startIdx < 0) startIdx = 0;
        if (startIdx + show > totalR) startIdx = totalR - show;
        if (startIdx < 0) startIdx = 0;

        for (int i = 0; i < show; i++) {
            int ri  = startIdx + i;
            bool sel = (ri == craft.selectedIndex());
            const Recipe* r = craft.filteredAt(ri);
            if (!r) continue;

            bool canDo = craft.canCraft(ri);
            unsigned int color = sel ? 0xffffff : (canDo ? 0x88ee88 : 0x886666);

            snprintf(buf, sizeof(buf), "%s%s → %dx %s  [%.1fs]",
                     sel ? "> " : "  ",
                     r->name,
                     r->resultCount, itemInfo(r->result).name,
                     r->craftTimeSecs);
            ov.drawText(buf, x, listY_ + i * ROW_H, color);
        }
        y = listY_ + show * ROW_H + 4;

        // ── Selected recipe details ───────────────────────────────────────────
        const Recipe* sel = craft.selectedRecipe();
        if (sel) {
            ov.drawText("INGREDIENTS:", x, y, 0xffcc44); y += LINE;
            for (int i = 0; i < sel->ingredientCount; i++) {
                const auto& ing = sel->ingredients[i];
                if (ing.item == ItemID::None) continue;
                int have = craft.inventory().countItem(ing.item);
                bool enough = (have >= ing.count);
                snprintf(buf, sizeof(buf), "  %s x%d  (have %d)",
                         itemInfo(ing.item).name, ing.count, have);
                ov.drawText(buf, x, y, enough ? 0x88ee88 : 0xff6644); y += LINE;
            }

            // Craft button
            bool can = craft.canCraftSelected() && !craft.isCrafting();
            craftBtnX_ = x; craftBtnY_ = y; craftBtnW_ = 120; craftBtnH_ = 18;
            UI::button(ov, craftBtnX_, craftBtnY_, craftBtnW_, craftBtnH_, can ? "CRAFT" : "NEED MATERIALS", can, UI::UIColorPalette::success_color);
            ov.drawText(craft.isCrafting() ? "CRAFTING..." : (can ? "[E] CRAFT" : "Need items"),
                        craftBtnX_ + 4, craftBtnY_ + 2,
                        can ? 0x44ff44 : 0x666666);

            // Progress bar
            if (craft.isCrafting()) {
                float pct = craft.craftProgress();
                ov.drawRect(craftBtnX_ + 128, craftBtnY_, 120, 18, 0x222222, 0.9f);
                ov.drawRect(craftBtnX_ + 128, craftBtnY_, (int)(120 * pct), 18, 0x224455, 0.9f);
                snprintf(buf, sizeof(buf), "%.0f%%", pct * 100.0f);
                ov.drawText(buf, craftBtnX_ + 132, craftBtnY_ + 2, 0x88ccff);
            }
            y += 24;
        }

        // ── Furnace section (only when station = Furnace) ─────────────────────
        if (craft.activeStation() == Station::Furnace) {
            y += 4;
            ov.drawText("FURNACE:", x, y, 0xff8844); y += LINE;
            const FurnaceState& fs = craft.furnace().state();

            snprintf(buf, sizeof(buf), "Fuel: %s x%d  (%.0fs left)",
                     fs.fuelItem != ItemID::None ? itemInfo(fs.fuelItem).name : "empty",
                     fs.fuelCount, fs.fuelTimeLeft);
            ov.drawText(buf, x, y, 0xffaa55); y += LINE;

            snprintf(buf, sizeof(buf), "Input: %s x%d   Output: %s x%d",
                     fs.inputItem  != ItemID::None ? itemInfo(fs.inputItem).name  : "empty",
                     fs.inputCount,
                     fs.outputItem != ItemID::None ? itemInfo(fs.outputItem).name : "empty",
                     fs.outputCount);
            ov.drawText(buf, x, y, 0xffcc88); y += LINE;

            if (craft.furnace().isActive()) {
                float sp = craft.furnace().smeltFraction();
                ov.drawRect(x, y, 160, 12, 0x222222, 0.9f);
                ov.drawRect(x, y, (int)(160 * sp), 12, 0xff6622, 0.9f);
                ov.drawText("Smelting...", x + 2, y + 1, 0xff8844);
                y += 16;
            }

            // Take output button
            if (craft.furnace().hasOutput()) {
                furnBtnX_ = x; furnBtnY_ = y; furnBtnW_ = 140; furnBtnH_ = 16;
                UI::button(ov, furnBtnX_, furnBtnY_, furnBtnW_, furnBtnH_, "TAKE OUTPUT", true, UI::UIColorPalette::success_color);
                ov.drawText("[T] Take Output", furnBtnX_ + 4, furnBtnY_ + 2, 0x44ff88);
                y += 20;
            }
        }

        // ── Status message ────────────────────────────────────────────────────
        const char* msg = craft.lastMessage();
        if (msg[0]) {
            ov.drawText(msg, x, panY + panH - 20, 0x00ffcc);
        }
        if (lastMsg_[0]) {
            ov.drawText(lastMsg_, x + 200, panY + panH - 20, 0xffaa44);
        }

        dirty_ = false;
    }

    // ── Debug info (for the debug overlay section in main) ───────────────────
    void drawDebugSection(DebugOverlay& ov, CraftManager& craft, int x, int& y) {
        char buf[128];
        ov.drawText("CRAFTING:", x, y, 0xffcc00); y += 13;

        snprintf(buf, sizeof(buf), "Station: %s  Recipe %d/%d",
                 craft.stationName(), craft.selectedIndex() + 1,
                 craft.filteredRecipeCount());
        ov.drawText(buf, x, y, 0xaaaaff); y += 13;

        const Recipe* r = craft.selectedRecipe();
        if (r) {
            snprintf(buf, sizeof(buf), "Selected: %s → %dx %s",
                     r->name, r->resultCount, itemInfo(r->result).name);
            ov.drawText(buf, x, y, craft.canCraftSelected() ? 0x88ee88 : 0x886666);
            y += 13;

            for (int i = 0; i < r->ingredientCount; i++) {
                const auto& ing = r->ingredients[i];
                if (ing.item == ItemID::None) continue;
                int have = craft.inventory().countItem(ing.item);
                snprintf(buf, sizeof(buf), "  %s: need %d have %d",
                         itemInfo(ing.item).name, ing.count, have);
                ov.drawText(buf, x, y, (have >= ing.count) ? 0x88ee88 : 0xff6644);
                y += 11;
            }
        }

        if (craft.isCrafting()) {
            snprintf(buf, sizeof(buf), "Crafting... %.0f%%", craft.craftProgress() * 100.0f);
            ov.drawText(buf, x, y, 0x44aaff); y += 13;
        }

        // Inventory summary
        int itemTypes = 0;
        for (int i = 0; i < Inventory::TOTAL_SLOTS; i++)
            if (craft.inventory().slot(i).item != ItemID::None) itemTypes++;
        snprintf(buf, sizeof(buf), "Inventory: %d/%d slots used",
                 itemTypes, Inventory::TOTAL_SLOTS);
        ov.drawText(buf, x, y, 0x888888); y += 13;

        const char* msg = craft.lastMessage();
        if (msg[0]) { ov.drawText(msg, x, y, 0x00ffcc); y += 13; }
        y += 4;
    }

private:
    bool open_  = false;
    bool dirty_ = false;
    char lastMsg_[64] = {};

    // Button and list rects (updated each draw call, used for click detection)
    static constexpr int ROW_H = 14;
    int listX_ = 0, listY_ = 0, listW_ = 0, listRowCount_ = 0;
    int craftBtnX_ = 0, craftBtnY_ = 0, craftBtnW_ = 0, craftBtnH_ = 0;
    int furnBtnX_  = 0, furnBtnY_  = 0, furnBtnW_  = 0, furnBtnH_  = 0;

    bool hitTest_(int mx, int my, int bx, int by, int bw, int bh) const {
        return mx >= bx && mx < bx + bw && my >= by && my < by + bh;
    }
};

} // namespace Craft
