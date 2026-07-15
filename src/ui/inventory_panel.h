#pragma once
#include "../craft/inventory.h"
#include "../rendering/debug_overlay.h"
#include <cstdio>
#include <cstring>

namespace UI {

class InventoryPanel {
public:
    bool isOpen() const { return open_; }
    void toggle() { open_ = !open_; anim_ = open_ ? 0.0f : 1.0f; clearDrag_(); }
    void close() { open_ = false; anim_ = 1.0f; clearDrag_(); }

    bool mobileButtonHit(int x, int y, int screenW, int screenH) const {
        int s = mobileButtonSize_(screenW, screenH);
        int bx = screenW - s - 14;
        int by = screenH - s - 92;
        return x >= bx && x < bx + s && y >= by && y < by + s;
    }

    void update(float dt) {
        float target = open_ ? 1.0f : 0.0f;
        float k = 1.0f - expf(-dt * 14.0f);
        anim_ += (target - anim_) * k;
        if (!open_ && anim_ < 0.01f) anim_ = 0.0f;
    }

    bool handleMouseDown(int x, int y, Craft::Inventory& inv, int screenW, int screenH) {
        if (!open_) return false;
        int slot = slotAt_(x, y, screenW, screenH);
        if (slot >= 0) {
            Craft::ItemSlot& s = inv.slot(slot);
            if (s.item != Craft::ItemID::None) {
                drag_ = s;
                dragFrom_ = slot;
                s = Craft::ItemSlot{};
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
        if (hasDrag_()) {
            int dst = slotAt_(x, y, screenW, screenH);
            if (dst >= 0) dropInto_(inv, dst);
            else restoreDrag_(inv);
            clearDrag_();
            return true;
        }
        return insidePanel_(x, y, screenW, screenH);
    }

    void drawMobileButton(DebugOverlay& ov, int screenW, int screenH) const {
        int s = mobileButtonSize_(screenW, screenH);
        int bx = screenW - s - 14;
        int by = screenH - s - 92;
        ov.drawRect(bx, by, s, s, open_ ? 0x3a4f6f : 0x111827, 0.82f);
        ov.drawRect(bx + 9, by + 11, s - 18, 5, 0xd8e4ff, 0.95f);
        ov.drawRect(bx + 9, by + 23, s - 18, 5, 0xd8e4ff, 0.95f);
        ov.drawRect(bx + 9, by + 35, s - 18, 5, 0xd8e4ff, 0.95f);
    }

    void draw(DebugOverlay& ov, const Craft::Inventory& inv, int screenW, int screenH) {
        if (anim_ <= 0.01f) return;
        int panW, panH, panX, panY;
        layout_(screenW, screenH, panW, panH, panX, panY);
        int slide = (int)((1.0f - anim_) * 28.0f);
        panY += slide;

        ov.drawRect(0, 0, screenW, screenH, 0x02040a, 0.30f * anim_);
        ov.drawRect(panX, panY, panW, panH, 0x101522, 0.96f * anim_);
        ov.drawRect(panX + 6, panY + 6, panW - 12, 34, 0x1f2937, 0.92f * anim_);
        ov.drawText("INVENTORY", panX + 16, panY + 17, 0xf5f7ff, 1.25f);
        ov.drawText("Search", panX + panW - 150, panY + 18, 0x8da2c0, 1.0f);
        ov.drawRect(panX + panW - 156, panY + 13, 136, 19, 0x0b1020, 0.95f * anim_);

        const char* cats[] = {"All", "Blocks", "Ore", "Craft", "Tools"};
        for (int i = 0; i < 5; ++i) {
            int cx = panX + 16 + i * 70;
            ov.drawRect(cx, panY + 48, 62, 18, i == category_ ? 0x31415f : 0x171d2b, 0.90f * anim_);
            ov.drawText(cats[i], cx + 7, panY + 53, i == category_ ? 0xffffff : 0x9ca8bf);
        }

        int startX = panX + (panW - (9 * SLOT + 8 * GAP)) / 2;
        int startY = panY + 82;
        for (int i = 0; i < Craft::Inventory::TOTAL_SLOTS; ++i) {
            int col = i % 9, row = i / 9;
            int sx = startX + col * (SLOT + GAP);
            int sy = startY + row * (SLOT + GAP);
            bool hot = i < Craft::Inventory::HOTBAR_SIZE;
            bool hov = (i == hoverSlot_);
            const Craft::ItemSlot& slot = inv.slot(i);
            unsigned bg = hot ? 0x202a3c : 0x151b28;
            if (hov) bg = 0x33425f;
            ov.drawRect(sx, sy, SLOT, SLOT, bg, 0.94f * anim_);
            ov.drawRect(sx, sy, SLOT, 2, hov ? 0xbfd7ff : 0x2a3446, 0.90f * anim_);
            ov.drawRect(sx, sy + SLOT - 2, SLOT, 2, 0x070a10, 0.85f * anim_);
            if (slot.item == Craft::ItemID::None) continue;
            drawItem_(ov, slot, sx + 6, sy + 6, SLOT - 12, anim_);
        }

        if (hoverSlot_ >= 0 && !hasDrag_()) drawTooltip_(ov, inv.slot(hoverSlot_), dragX_, dragY_, screenW, screenH);
        if (hasDrag_()) drawItem_(ov, drag_, dragX_ - 19, dragY_ - 19, 38, anim_);

        ov.drawText("E close", panX + 16, panY + panH - 22, 0x7f8da8);
        ov.drawText("Drag to move  |  same item merges stacks", panX + 88, panY + panH - 22, 0x7f8da8);
    }

private:
    static constexpr int SLOT = 46;
    static constexpr int GAP = 5;
    bool open_ = false;
    float anim_ = 0.0f;
    int category_ = 0;
    int hoverSlot_ = -1;
    int dragFrom_ = -1;
    int dragX_ = 0, dragY_ = 0;
    Craft::ItemSlot drag_;

    bool hasDrag_() const { return drag_.item != Craft::ItemID::None && drag_.count > 0; }
    void clearDrag_() { drag_ = Craft::ItemSlot{}; dragFrom_ = -1; }
    void restoreDrag_(Craft::Inventory& inv) { if (dragFrom_ >= 0) inv.slot(dragFrom_) = drag_; }

    void dropInto_(Craft::Inventory& inv, int dst) {
        Craft::ItemSlot& t = inv.slot(dst);
        if (t.item == Craft::ItemID::None) { t = drag_; return; }
        const Craft::ItemInfo& info = Craft::itemInfo(t.item);
        if (t.item == drag_.item && info.maxStack > 1 && t.count < info.maxStack) {
            int space = info.maxStack - t.count;
            int take = drag_.count < space ? drag_.count : space;
            t.count += (uint16_t)take;
            drag_.count -= (uint16_t)take;
            if (drag_.count > 0) restoreDrag_(inv);
            return;
        }
        if (dragFrom_ >= 0) inv.slot(dragFrom_) = t;
        t = drag_;
    }

    static int mobileButtonSize_(int screenW, int) { return screenW < 560 ? 52 : 46; }
    static void layout_(int screenW, int screenH, int& panW, int& panH, int& panX, int& panY) {
        panW = screenW < 620 ? screenW - 24 : 560;
        panH = 316;
        if (screenH < 410) panH = screenH - 24;
        panX = (screenW - panW) / 2;
        panY = (screenH - panH) / 2;
    }
    static bool insidePanel_(int x, int y, int screenW, int screenH) {
        int w,h,px,py; layout_(screenW, screenH, w,h,px,py);
        return x >= px && x < px + w && y >= py && y < py + h;
    }
    static int slotAt_(int x, int y, int screenW, int screenH) {
        int panW, panH, panX, panY; layout_(screenW, screenH, panW, panH, panX, panY);
        int startX = panX + (panW - (9 * SLOT + 8 * GAP)) / 2;
        int startY = panY + 82;
        for (int i = 0; i < Craft::Inventory::TOTAL_SLOTS; ++i) {
            int col = i % 9, row = i / 9;
            int sx = startX + col * (SLOT + GAP);
            int sy = startY + row * (SLOT + GAP);
            if (x >= sx && x < sx + SLOT && y >= sy && y < sy + SLOT) return i;
        }
        return -1;
    }
    static unsigned itemColor_(Craft::ItemID id) {
        const Craft::ItemInfo& info = Craft::itemInfo(id);
        if (info.placeAs != BLOCK_AIR) {
            const BlockColor& c = BLOCK_COLORS[(int)info.placeAs];
            return ((unsigned)(c.r * 255.0f) << 16) | ((unsigned)(c.g * 255.0f) << 8) | (unsigned)(c.b * 255.0f);
        }
        switch (info.kind) {
            case Craft::ItemKind::Ore: return 0xb7a657;
            case Craft::ItemKind::Ingot: return 0xd98a4a;
            case Craft::ItemKind::Craft: return 0x6fb36f;
            case Craft::ItemKind::Tool: return 0x5aa7d8;
            case Craft::ItemKind::Station: return 0x9a744e;
            default: return 0x777f91;
        }
    }
    static void drawItem_(DebugOverlay& ov, const Craft::ItemSlot& s, int x, int y, int size, float a) {
        unsigned c = itemColor_(s.item);
        ov.drawRect(x, y, size, size, c, 0.92f * a);
        ov.drawRect(x, y, size, 5, 0xffffff, 0.16f * a);
        ov.drawRect(x, y, 5, size, 0xffffff, 0.10f * a);
        if (s.count > 1) {
            char buf[8]; snprintf(buf, sizeof(buf), "%d", s.count);
            int tw = (int)strlen(buf) * 8;
            ov.drawRect(x + size - tw - 4, y + size - 13, tw + 4, 12, 0x000000, 0.55f * a);
            ov.drawText(buf, x + size - tw - 2, y + size - 12, 0xffffff);
        }
    }
    static void drawTooltip_(DebugOverlay& ov, const Craft::ItemSlot& s, int mx, int my, int screenW, int screenH) {
        if (s.item == Craft::ItemID::None) return;
        const Craft::ItemInfo& info = Craft::itemInfo(s.item);
        int len = (int)strlen(info.name);
        int w = len * 8 + 18;
        int x = mx + 16, y = my + 16;
        if (x + w > screenW) x = mx - w - 12;
        if (y + 34 > screenH) y = my - 38;
        ov.drawRect(x, y, w, 32, 0x05070d, 0.92f);
        ov.drawText(info.name, x + 8, y + 7, 0xffffff);
        const char* kind = "Item";
        switch (info.kind) {
            case Craft::ItemKind::Block: kind = "Block"; break;
            case Craft::ItemKind::Ore: kind = "Ore"; break;
            case Craft::ItemKind::Ingot: kind = "Ingot"; break;
            case Craft::ItemKind::Craft: kind = "Craft"; break;
            case Craft::ItemKind::Tool: kind = "Tool"; break;
            case Craft::ItemKind::Station: kind = "Station"; break;
            default: break;
        }
        ov.drawText(kind, x + 8, y + 19, 0x9ca8bf);
    }
};

} // namespace UI
