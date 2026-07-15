#pragma once
// craft_save.h — Binary persistence for inventory + furnace state.
// Format: magic "CRAF" + uint32 version=1 + slot array + furnace state.
#include "craft_manager.h"
#include <cstdio>
#include <cstring>
#include "SDL.h"

namespace Craft {

static constexpr uint32_t CRAFT_SAVE_VERSION = 1u;

inline bool saveCraft(const CraftManager& mgr, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    // Header
    const char magic[4] = {'C','R','A','F'};
    fwrite(magic, 4, 1, f);
    fwrite(&CRAFT_SAVE_VERSION, 4, 1, f);

    // Inventory slots
    uint8_t slotCount = (uint8_t)Inventory::TOTAL_SLOTS;
    fwrite(&slotCount, 1, 1, f);
    const Inventory& inv = mgr.inventory();
    for (int i = 0; i < Inventory::TOTAL_SLOTS; i++) {
        const ItemSlot& s = inv.slot(i);
        uint16_t id  = (uint16_t)s.item;
        uint16_t cnt = s.count;
        uint16_t dur = s.durability;
        fwrite(&id,  2, 1, f);
        fwrite(&cnt, 2, 1, f);
        fwrite(&dur, 2, 1, f);
    }

    // Furnace state
    const FurnaceState& fs = mgr.furnace().state();
    uint16_t fi  = (uint16_t)fs.fuelItem;
    uint16_t fc  = fs.fuelCount;
    uint16_t ii  = (uint16_t)fs.inputItem;
    uint16_t ic  = fs.inputCount;
    uint16_t oi  = (uint16_t)fs.outputItem;
    uint16_t oc  = fs.outputCount;
    float    ftl = fs.fuelTimeLeft;
    float    sa  = fs.smeltAccum;
    fwrite(&fi, 2, 1, f); fwrite(&fc, 2, 1, f);
    fwrite(&ii, 2, 1, f); fwrite(&ic, 2, 1, f);
    fwrite(&oi, 2, 1, f); fwrite(&oc, 2, 1, f);
    fwrite(&ftl, 4, 1, f);
    fwrite(&sa,  4, 1, f);

    fclose(f);
    SDL_Log("[CraftSave] Saved to %s", path);
    return true;
}

inline bool loadCraft(CraftManager& mgr, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    char     magic[4] = {};
    uint32_t version  = 0;
    fread(magic, 4, 1, f);
    fread(&version, 4, 1, f);

    bool ok = (magic[0]=='C' && magic[1]=='R' && magic[2]=='A' && magic[3]=='F')
           && (version == CRAFT_SAVE_VERSION);

    if (ok) {
        uint8_t slotCount = 0;
        fread(&slotCount, 1, 1, f);
        Inventory& inv = mgr.inventory();
        inv.clear();
        for (int i = 0; i < slotCount && i < Inventory::TOTAL_SLOTS; i++) {
            uint16_t id=0, cnt=0, dur=0;
            fread(&id,  2, 1, f);
            fread(&cnt, 2, 1, f);
            fread(&dur, 2, 1, f);
            if ((int)id < ITEM_COUNT && cnt > 0) {
                inv.slot(i).item       = (ItemID)id;
                inv.slot(i).count      = cnt;
                inv.slot(i).durability = dur;
            }
        }

        // Furnace state
        uint16_t fi=0,fc=0,ii=0,ic=0,oi=0,oc=0;
        float ftl=0.0f, sa=0.0f;
        fread(&fi, 2, 1, f); fread(&fc, 2, 1, f);
        fread(&ii, 2, 1, f); fread(&ic, 2, 1, f);
        fread(&oi, 2, 1, f); fread(&oc, 2, 1, f);
        fread(&ftl, 4, 1, f);
        fread(&sa,  4, 1, f);

        FurnaceState& fs = mgr.furnace().state();
        fs.fuelItem    = (fi < (uint16_t)ITEM_COUNT) ? (ItemID)fi : ItemID::None;
        fs.fuelCount   = fc;
        fs.inputItem   = (ii < (uint16_t)ITEM_COUNT) ? (ItemID)ii : ItemID::None;
        fs.inputCount  = ic;
        fs.outputItem  = (oi < (uint16_t)ITEM_COUNT) ? (ItemID)oi : ItemID::None;
        fs.outputCount = oc;
        fs.fuelTimeLeft= ftl;
        fs.smeltAccum  = sa;

        SDL_Log("[CraftSave] Loaded from %s", path);
    }

    fclose(f);
    return ok;
}

} // namespace Craft
