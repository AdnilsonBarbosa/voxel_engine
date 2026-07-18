#pragma once
#include "../craft/item_defs.h"
#include "../craft/inventory.h"
#include "../rendering/debug_overlay.h"
#include "../rendering/texture_atlas.h"
#include "block_icon.h"
#include "ui_theme.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace UI {

class Hotbar {
public:
    static constexpr int SLOTS = Craft::Inventory::HOTBAR_SIZE;
    static constexpr int GAP = 5;
    // Touch mode gets finger-sized slots at runtime (any platform).
    static int slotBase() { return UITheme::touchUI() ? 84 : 46; }
    static int barH()     { return UITheme::touchUI() ? 108 : 62; }

    static int slotSize(int screenW) {
        const int fit = (screenW - 36 - (SLOTS - 1) * 4) / SLOTS;
        return std::max(44, std::min(slotBase(), fit));
    }
    static int gap(int screenW) { return screenW < 520 ? 4 : GAP; }
    static int panelWidth(int screenW) {
        const int s = slotSize(screenW), g = gap(screenW);
        return SLOTS * s + (SLOTS - 1) * g + 18;
    }
    static int panelY(int screenH) { return screenH - barH() - UITheme::bottomInset(screenH); }

    void update(float dt, int selectedSlot) {
        if (selectedSlot != lastSelected_) {
            lastSelected_ = selectedSlot;
            nameTimer_ = 2.0f;
            selectionAnim_ = 0.0f;
        }
        selectionAnim_ += (1.0f - selectionAnim_) * (1.0f - expf(-dt * 14.0f));
        if (nameTimer_ > 0.0f) nameTimer_ -= dt;
    }

    void draw(DebugOverlay& ov, const TextureAtlas& atlas,
              const Craft::Inventory& inv, int screenW, int screenH) {
        const int s = slotSize(screenW), g = gap(screenW);
        const int panW = panelWidth(screenW);
        const int panX = (screenW - panW) / 2;
        const int panY = panelY(screenH);
        panel(ov, panX, panY, panW, barH(), 0.72f);

        for (int i = 0; i < SLOTS; ++i) {
            const Craft::ItemSlot& item = inv.slot(i);
            const bool selected = i == inv.selectedSlot();
            const int grow = selected ? (int)(3.0f * selectionAnim_) : 0;
            const int drawS = s + grow;
            const int x = panX + 9 + i * (s + g) - grow / 2;
            const int y = panY + 10 - grow / 2;
            slot(ov, x, y, drawS, selected);

            if (item.item == Craft::ItemID::None) {
                char key[4]; snprintf(key, sizeof(key), "%d", i + 1);
                ov.drawText(key, x + 6, y + 6, selected ? UIColorPalette::selected_color : UIColorPalette::text_muted,
                            1.20f);
                continue;
            }

            const int icon = drawS - 20;
            drawCachedItemIcon(ov, atlas, iconCache_, item.item,
                               x + 10, y + 10,
                               icon, 1.0f);

            if (item.count > 1) {
                char count[8]; snprintf(count, sizeof(count), "%d", item.count);
                const int tw = UITheme::textWidth(count, UITypography::body_small);
                roundedFill(ov, x + drawS - tw - 7, y + drawS - 15, tw + 5, 12,
                            UIColorPalette::shadow_color, 0.76f, 4);
                ov.drawText(count, x + drawS - tw - 4, y + drawS - 14,
                            UIColorPalette::text_primary, 1.20f);
            }

            const Craft::ItemInfo& info = Craft::itemInfo(item.item);
            if (info.maxDurability > 0 && item.durability < info.maxDurability) {
                const int width = drawS - 12;
                const int filled = (int)(width * (float)item.durability / info.maxDurability);
                ov.drawRect(x + 6, y + drawS - 6, width, 3, UIColorPalette::shadow_color, 0.9f);
                ov.drawRect(x + 6, y + drawS - 6, filled, 3,
                            item.durability > info.maxDurability / 2 ? UIColorPalette::success_color :
                            UIColorPalette::warning_color, 0.95f);
            }
        }

        const Craft::ItemSlot& held = inv.slot(inv.selectedSlot());
        if (held.item != Craft::ItemID::None && nameTimer_ > 0.0f) {
            const char* name = Craft::itemInfo(held.item).name;
            const int tw = UITheme::textWidth(name, UITypography::body);
            const int nx = (screenW - tw - 22) / 2;
            const int ny = panY - 34;
            roundedFill(ov, nx, ny, tw + 28, 30, UIColorPalette::background_primary, 0.90f, 8);
            ov.drawText(name, nx + 11, ny + 6, UIColorPalette::text_primary, 1.20f);
        }
    }

    int lastSelected_ = -1;
    float selectionAnim_ = 1.0f;
    float nameTimer_ = 0.0f;

private:
    ItemIconCache iconCache_;
};

} // namespace UI
