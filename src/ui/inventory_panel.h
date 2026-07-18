#pragma once
#include "../craft/inventory.h"
#include "../rendering/debug_overlay.h"
#include "../rendering/texture_atlas.h"
#include "block_icon.h"
#include "hotbar.h"
#include "ui_theme.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace UI {

class InventoryPanel {
public:
    bool isOpen() const { return open_; }
    void toggle() { open_ = !open_; anim_ = open_ ? 0.0f : 1.0f; clearDrag_(); }
    void close() { open_ = false; anim_ = 1.0f; clearDrag_(); }

    bool mobileButtonHit(int x, int y, int screenW, int screenH) const {
        const int s = buttonSize_(screenW);
        int bx, by; launcherPos_(screenW, screenH, s, bx, by);
        return x >= bx - 8 && x < bx + s + 8 && y >= by - 8 && y < by + s + 8;
    }

    void update(float dt) {
        const float target = open_ ? 1.0f : 0.0f;
        anim_ += (target - anim_) * (1.0f - expf(-dt * 16.0f));
        if (!open_ && anim_ < 0.01f) anim_ = 0.0f;
    }

    bool handleMouseDown(int x, int y, Craft::Inventory& inv, int screenW, int screenH) {
        if (!open_) return false;
        if (closeHit_(x, y, screenW, screenH)) { restoreDrag_(inv); close(); return true; }
        const int cat = categoryAt_(x, y, screenW, screenH);
        if (cat >= 0) { category_ = cat; hoverSlot_ = -1; restoreDrag_(inv); clearDrag_(); return true; }
        const int s = slotAt_(x, y, screenW, screenH);
        // Sticky move (mobile): an item picked with a tap travels to the next
        // tapped slot — no drag precision needed.
        if (sticky_ && hasDrag_()) {
            if (s >= 0) dropInto_(inv, s); else restoreDrag_(inv);
            clearDrag_();
            return s >= 0 || insidePanel_(x, y, screenW, screenH);
        }
        if (s >= 0) {
            Craft::ItemSlot& slotRef = inv.slot(s);
            if (slotRef.item != Craft::ItemID::None) {
                drag_ = slotRef; dragFrom_ = s; slotRef = Craft::ItemSlot{};
                dragX_ = x; dragY_ = y;
            }
            return true;
        }
        return insidePanel_(x, y, screenW, screenH);
    }

    bool handleMouseMove(int x, int y, int screenW, int screenH) {
        if (!open_) return false;
        hoverSlot_ = slotAt_(x, y, screenW, screenH);
        dragX_ = x; dragY_ = y;
        return insidePanel_(x, y, screenW, screenH) || hasDrag_();
    }

    bool handleMouseUp(int x, int y, Craft::Inventory& inv, int screenW, int screenH) {
        if (!open_) return false;
        if (hasDrag_() && !sticky_) {
            const int dst = slotAt_(x, y, screenW, screenH);
            if (dst == dragFrom_) { sticky_ = true; return true; }  // tap → armed
            if (dst >= 0) dropInto_(inv, dst); else restoreDrag_(inv);
            clearDrag_();
            return true;
        }
        return insidePanel_(x, y, screenW, screenH);
    }

    void drawMobileButton(DebugOverlay& ov, int screenW, int screenH) const {
        const int s = buttonSize_(screenW);
        int bx, by; launcherPos_(screenW, screenH, s, bx, by);
        iconButton(ov, bx, by, s, UIColorPalette::selected_color, 0, open_);
    }

    // Launcher sits at the right end of the hotbar (like Bedrock's "..." slot)
    // instead of floating over the action buttons.
    static void launcherPos_(int W, int H, int s, int& bx, int& by) {
        bx = (W + Hotbar::panelWidth(W)) / 2 + 10;
        if (bx + s > W - 6) bx = W - s - 6;
        by = Hotbar::panelY(H) + (Hotbar::barH() - s) / 2;
    }

    void draw(DebugOverlay& ov, const TextureAtlas& atlas,
              const Craft::Inventory& inv, int screenW, int screenH) {
        if (anim_ <= 0.01f) return;
        int panW, panH, panX, panY;
        layout_(screenW, screenH, panW, panH, panX, panY);
        const int slide = (int)((1.0f - anim_) * 22.0f);
        panY += slide;

        roundedFill(ov, 0, 0, screenW, screenH, UIColorPalette::shadow_color, 0.34f * anim_, 0);
        panel(ov, panX, panY, panW, panH, 0.92f * anim_);
        header(ov, panX, panY, panW, "INVENTORY", "TOUCH AN ITEM TO MOVE");
        iconButton(ov, panX + panW - 46, panY + 8, 34, UIColorPalette::text_primary, 2, false);

        const char* cats[] = {"ALL", "NATURAL", "WOOD", "STONE", "BUILD", "DECOR", "TOOLS", "FOOD"};
        const int catY = panY + 50;
        const int catW = std::max(56, (panW - 24) / 5);
        for (int i = 0; i < 8; ++i) {
            const int row = i / 5, col = i % 5;
            const int cx = panX + 12 + col * catW;
            const int cy = catY + row * 28;
            button(ov, cx, cy, catW - 5, 23, cats[i], category_ == i);
        }

        const int gridTop = catY + 64;
        const int columns = columns_(screenW);
        const int size = gridSlotSize_(panW, columns);
        const int gap = size < 46 ? 4 : 6;
        const int gridW = columns * size + (columns - 1) * gap;
        const int startX = panX + (panW - gridW) / 2;
        const int rows = (Craft::Inventory::TOTAL_SLOTS - Craft::Inventory::HOTBAR_SIZE + columns - 1) / columns;
        for (int i = Craft::Inventory::HOTBAR_SIZE; i < Craft::Inventory::TOTAL_SLOTS; ++i) {
            const int n = i - Craft::Inventory::HOTBAR_SIZE;
            drawSlot_(ov, atlas, inv.slot(i), i, startX + (n % columns) * (size + gap),
                      gridTop + (n / columns) * (size + gap), size, inv);
        }

        const int hotY = std::min(panY + panH - size - 28, gridTop + rows * (size + gap) + 14);
        ov.drawText("HOTBAR", startX, hotY - 13, UIColorPalette::text_muted, UITypography::hint);
        for (int i = 0; i < Craft::Inventory::HOTBAR_SIZE; ++i)
            drawSlot_(ov, atlas, inv.slot(i), i, startX + (i % columns) * (size + gap),
                      hotY, size, inv);

        if (hoverSlot_ >= 0 && !hasDrag_())
            drawTooltip_(ov, inv.slot(hoverSlot_), dragX_, dragY_, screenW, screenH);
        if (hasDrag_()) {
            const int ix = dragX_ - size / 2;
            const int iy = dragY_ - size / 2 - (sticky_ ? 26 : 0);
            if (sticky_)
                roundedFill(ov, ix - 5, iy - 5, size + 6, size + 6,
                            UIColorPalette::selected_color, 0.45f, 8);
            drawCachedItemIcon(ov, atlas, iconCache_, drag_.item, ix, iy, size - 4, 1.0f);
        }

        ov.drawText(sticky_ ? "Item picked  •  touch the destination slot"
                            : "Touch an item, then touch where it goes",
                    panX + 14, panY + panH - 15, UIColorPalette::text_muted, UITypography::hint);
    }

private:
    static constexpr int CATEGORY_COUNT = 8;
    bool open_ = false;
    float anim_ = 0.0f;
    int category_ = 0;
    int hoverSlot_ = -1;
    int dragFrom_ = -1;
    bool sticky_ = false;
    int dragX_ = 0, dragY_ = 0;
    Craft::ItemSlot drag_{};
    ItemIconCache iconCache_;

    bool hasDrag_() const { return drag_.item != Craft::ItemID::None && drag_.count > 0; }
    void clearDrag_() { drag_ = Craft::ItemSlot{}; dragFrom_ = -1; sticky_ = false; }
    void restoreDrag_(Craft::Inventory& inv) { if (dragFrom_ >= 0) inv.slot(dragFrom_) = drag_; }

    void dropInto_(Craft::Inventory& inv, int dst) {
        Craft::ItemSlot& target = inv.slot(dst);
        if (target.item == Craft::ItemID::None) { target = drag_; return; }
        const Craft::ItemInfo& info = Craft::itemInfo(target.item);
        if (target.item == drag_.item && info.maxStack > 1 && target.count < info.maxStack) {
            const int take = std::min((int)drag_.count, (int)info.maxStack - (int)target.count);
            target.count += (uint16_t)take; drag_.count -= (uint16_t)take;
            if (drag_.count > 0) restoreDrag_(inv);
            return;
        }
        if (dragFrom_ >= 0) inv.slot(dragFrom_) = target;
        target = drag_;
    }

    // Touch mode (runtime, any platform) gets finger-sized targets.
    static int buttonSize_(int screenW) {
        if (UITheme::touchUI()) return 96;
        return screenW < 560 ? 48 : 42;
    }
    static int columns_(int screenW) {
        if (UITheme::touchUI()) return screenW < 900 ? 6 : 9;
        return screenW < 500 ? 5 : (screenW < 900 ? 7 : 9);
    }
    static int gridSlotSize_(int panW, int cols) {
        if (UITheme::touchUI())
            return std::max(56, std::min(82, (panW - 42 - (cols - 1) * 8) / cols));
        return std::max(34, std::min(52, (panW - 30 - (cols - 1) * 6) / cols));
    }

    static void layout_(int screenW, int screenH, int& w, int& h, int& x, int& y) {
        const int edge = UITheme::edge(screenW);
        w = (int)(screenW * 0.92f);
        if (!UITheme::touchUI() && w > 760) w = 760;
        if (w < 300) w = screenW - edge * 2;
        h = (int)(screenH * 0.88f);
        if (!UITheme::touchUI() && h > 570) h = 570;
        if (h < 380) h = screenH - edge * 2;
        x = (screenW - w) / 2;
        y = (screenH - h) / 2;
    }
    static bool insidePanel_(int x, int y, int sw, int sh) {
        int w,h,px,py; layout_(sw,sh,w,h,px,py);
        return x >= px && x < px + w && y >= py && y < py + h;
    }
    static bool closeHit_(int x, int y, int sw, int sh) {
        int w,h,px,py; layout_(sw,sh,w,h,px,py);
        return x >= px + w - 58 && x < px + w - 4 && y >= py + 3 && y < py + 50;
    }
    static int categoryAt_(int x, int y, int sw, int sh) {
        int w,h,px,py; layout_(sw,sh,w,h,px,py);
        const int catW = std::max(56, (w - 24) / 5);
        for (int i = 0; i < CATEGORY_COUNT; ++i) {
            const int row = i / 5, col = i % 5;
            const int cx = px + 12 + col * catW, cy = py + 50 + row * 28;
            if (x >= cx && x < cx + catW - 5 && y >= cy && y < cy + 23) return i;
        }
        return -1;
    }
    static int slotAt_(int x, int y, int sw, int sh) {
        int w,h,px,py; layout_(sw,sh,w,h,px,py);
        const int cols = columns_(sw), size = gridSlotSize_(w, cols), gap = size < 46 ? 4 : 6;
        const int gridW = cols * size + (cols - 1) * gap;
        const int startX = px + (w - gridW) / 2, gridTop = py + 114;
        const int rows = (27 + cols - 1) / cols;
        for (int i = 9; i < 36; ++i) {
            const int n = i - 9, sx = startX + (n % cols) * (size + gap), sy = gridTop + (n / cols) * (size + gap);
            if (x >= sx && x < sx + size && y >= sy && y < sy + size) return i;
        }
        const int hotY = std::min(py + h - size - 28, gridTop + rows * (size + gap) + 14);
        for (int i = 0; i < 9; ++i) {
            const int sx = startX + (i % cols) * (size + gap);
            if (x >= sx && x < sx + size && y >= hotY && y < hotY + size) return i;
        }
        return -1;
    }
    bool matchesCategory_(const Craft::ItemSlot& s) const {
        if (category_ == 0 || s.item == Craft::ItemID::None) return true;
        const auto kind = Craft::itemInfo(s.item).kind;
        switch (category_) {
            case 1: return kind == Craft::ItemKind::Block || kind == Craft::ItemKind::Ore;
            case 2: return kind == Craft::ItemKind::Craft || kind == Craft::ItemKind::Station;
            case 3: return kind == Craft::ItemKind::Block && Craft::itemInfo(s.item).placeAs == BLOCK_STONE;
            case 4: return kind == Craft::ItemKind::Block;
            case 5: return kind == Craft::ItemKind::Craft || kind == Craft::ItemKind::Station;
            case 6: return kind == Craft::ItemKind::Tool;
            case 7: return kind == Craft::ItemKind::Craft;
            default: return true;
        }
    }
    void drawSlot_(DebugOverlay& ov, const TextureAtlas& atlas, const Craft::ItemSlot& s,
                   int index, int x, int y, int size, const Craft::Inventory& inv) {
        const bool selected = index == inv.selectedSlot();
        slot(ov, x, y, size, selected, !matchesCategory_(s));
        if (s.item == Craft::ItemID::None) return;
        drawCachedItemIcon(ov, atlas, iconCache_, s.item, x + 3, y + 3, size - 6,
                           matchesCategory_(s) ? 1.0f : 0.30f);
        if (s.count > 1) {
            char buf[8]; snprintf(buf, sizeof(buf), "%d", s.count);
            const int tw = UITheme::textWidth(buf, UITypography::body_small);
            roundedFill(ov, x + size - tw - 7, y + size - 17, tw + 5, 14,
                        UIColorPalette::shadow_color, 0.78f, 4);
            ov.drawText(buf, x + size - tw - 4, y + size - 15,
                        UIColorPalette::text_primary, UITypography::body_small);
        }
    }
    static void drawTooltip_(DebugOverlay& ov, const Craft::ItemSlot& s,
                             int mx, int my, int sw, int sh) {
        if (s.item == Craft::ItemID::None) return;
        const char* name = Craft::itemInfo(s.item).name;
        int w = UITheme::textWidth(name, UITypography::body) + 22;
        int x = mx + 14, y = my + 14;
        if (x + w > sw) x = mx - w - 10;
        if (y + 38 > sh) y = my - 44;
        roundedFill(ov, x, y, w, 36, UIColorPalette::background_primary, 0.95f, 7);
        ov.drawText(name, x + 10, y + 7, UIColorPalette::text_primary, UITypography::body);
        ov.drawText(Craft::itemInfo(s.item).kind == Craft::ItemKind::Tool ? "TOOL" : "BLOCK",
                    x + 10, y + 21, UIColorPalette::text_secondary, UITypography::hint);
    }
};

} // namespace UI
