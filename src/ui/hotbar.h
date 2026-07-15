#pragma once
// hotbar.h — Minecraft-style hotbar at screen bottom.
// Shows 9 inventory slots with block colour previews, counts, and durability bars.
// Drawn via DebugOverlay — no extra GL program needed.
#include "../craft/item_defs.h"
#include "../craft/inventory.h"
#include "../rendering/debug_overlay.h"
#include "../world/block_types.h"
#include <cstdio>
#include <cstring>

namespace UI {

class Hotbar {
public:
    static constexpr int SLOTS   = Craft::Inventory::HOTBAR_SIZE; // 9
    static constexpr int SZ      = 52;   // slot outer size (px)
    static constexpr int GAP     = 3;    // gap between slots
    static constexpr int ICON    = 40;   // coloured block icon size
    static constexpr int BAR_H   = 68;   // panel height

    // ── Draw the hotbar and held-item name ────────────────────────────────────
    void draw(DebugOverlay& ov, const Craft::Inventory& inv,
              int screenW, int screenH) {
        int panW  = SLOTS * SZ + (SLOTS - 1) * GAP + 12;
        int panX  = (screenW - panW) / 2;
        int panY  = screenH - BAR_H - 4;
        int sel   = inv.selectedSlot();

        // Background panel
        ov.drawRect(panX, panY, panW, BAR_H, 0x0a0a18, 0.88f);

        for (int i = 0; i < SLOTS; i++) {
            int sx = panX + 6 + i * (SZ + GAP);
            int sy = panY + 8;

            const Craft::ItemSlot& slot = inv.slot(i);
            bool isSel = (i == sel);

            // Slot background
            ov.drawRect(sx, sy, SZ, SZ, isSel ? 0x2a2a44u : 0x141422u, 0.92f);

            // Selection border (4 thin rects)
            if (isSel) {
                ov.drawRect(sx,        sy,        SZ,  2, 0xffffff, 1.0f);
                ov.drawRect(sx,        sy+SZ-2,   SZ,  2, 0xffffff, 1.0f);
                ov.drawRect(sx,        sy,        2, SZ, 0xffffff, 1.0f);
                ov.drawRect(sx+SZ-2,   sy,        2, SZ, 0xffffff, 1.0f);
            }

            if (slot.item == Craft::ItemID::None) {
                // Show slot number in empty slot
                char num[4]; snprintf(num, sizeof(num), "%d", i + 1);
                ov.drawText(num, sx + 4, sy + 4, 0x333355);
                continue;
            }

            // ── Coloured block icon ───────────────────────────────────────────
            unsigned col = itemColour_(slot.item);
            int iconX = sx + (SZ - ICON) / 2;
            int iconY = sy + (SZ - ICON) / 2;
            ov.drawRect(iconX, iconY, ICON, ICON, col, 0.96f);

            // Highlight sheen (top-left triangle effect = two rects)
            ov.drawRect(iconX, iconY, ICON, 5, col | 0x606060u, 0.30f);
            ov.drawRect(iconX, iconY, 5, ICON, col | 0x606060u, 0.30f);

            // ── Stack count (bottom-right of icon) ────────────────────────────
            if (slot.count > 1) {
                char cnt[8]; snprintf(cnt, sizeof(cnt), "%d", slot.count);
                int cx = iconX + ICON - (int)strlen(cnt) * 8;
                ov.drawRect(cx - 1, iconY + ICON - 12, (int)strlen(cnt) * 8 + 2, 11, 0x000000, 0.55f);
                ov.drawText(cnt, cx, iconY + ICON - 12, 0xffffff);
            }

            // ── Durability bar (tool items) ───────────────────────────────────
            const Craft::ItemInfo& info = Craft::itemInfo(slot.item);
            if (info.maxDurability > 0 && slot.durability < info.maxDurability) {
                int filled = (int)(SZ * (float)slot.durability / info.maxDurability);
                unsigned durCol = filled > SZ * 2 / 3 ? 0x44ee44u :
                                  filled > SZ / 3     ? 0xffaa00u : 0xff3333u;
                ov.drawRect(sx,         sy + SZ - 4, SZ, 3, 0x222222, 0.9f);
                ov.drawRect(sx,         sy + SZ - 4, filled, 3, durCol, 0.9f);
            }
        }

        // ── Item name above hotbar ─────────────────────────────────────────────
        const Craft::ItemSlot& held = inv.slot(sel);
        if (held.item != Craft::ItemID::None) {
            const char* nm = Craft::itemInfo(held.item).name;
            int len  = (int)strlen(nm);
            int nx   = (screenW - len * 8) / 2;
            int ny   = panY - 18;
            ov.drawRect(nx - 5, ny - 2, len * 8 + 10, 14, 0x000000, 0.65f);
            ov.drawText(nm, nx, ny, 0xffffff);

            // Hint: scroll to switch, RMB to place
            ov.drawText("Scroll=switch  RMB=place  LMB=mine",
                        panX, panY + BAR_H + 2, 0x444466);
        }

        // ── Crafting status (compact — replaces the old panel) ───────────────
        // (intentionally empty: craft state visible via log; UI kept minimal)
    }

private:
    // Convert ItemID → 0xRRGGBB colour for the icon
    static unsigned itemColour_(Craft::ItemID id) {
        using Craft::ItemInfo;
        using Craft::ItemKind;
        const ItemInfo& info = Craft::itemInfo(id);

        if (info.placeAs != BLOCK_AIR) {
            const BlockColor& bc = BLOCK_COLORS[(int)info.placeAs];
            auto r = (unsigned)(bc.r * 255.0f) & 0xff;
            auto g = (unsigned)(bc.g * 255.0f) & 0xff;
            auto b = (unsigned)(bc.b * 255.0f) & 0xff;
            return (r << 16) | (g << 8) | b;
        }

        switch (info.kind) {
            case ItemKind::Ore:     return 0xaaaa44u; // dark yellow (raw ore)
            case ItemKind::Ingot:   return 0xff8833u; // orange  (ingot)
            case ItemKind::Craft:   return 0x88bb55u; // lime    (planks/sticks/torch)
            case ItemKind::Tool:    return 0x55aaffu; // steel blue
            case ItemKind::Station: return 0x886644u; // wood brown
            default:                return 0x777788u;
        }
    }
};

} // namespace UI
