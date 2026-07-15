#pragma once
#include "block_types.h"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstdio>
#include <cmath>

// One block modification stored in the world save.
struct BlockOverride {
    int32_t wx, wy, wz;
    uint8_t block;
};

// Persistent world state: starter-village placement + all player block edits.
//
// Thread safety model:
//   - Populated entirely on the main thread (constructor, setBlock).
//   - byChunk is read by workers but only after it is fully built (before
//     workers start). New overrides added while workers are running are only
//     needed for future chunk loads, so no lock is needed on the read path.
class WorldSave {
public:
    static constexpr uint32_t MAGIC   = 0x564F5853u; // "VOXS"
    static constexpr uint32_t VERSION = 1u;
    static constexpr int      CW      = 16;          // chunk width (== CHUNK_W)

    bool villagePlaced    = false;
    int  villageOriginX   = 0;
    int  villageOriginZ   = 0;
    int  villageBlocksOut = 0;   // non-air blocks stamped (for debug)

    // Full ordered list of overrides (village first, then player edits).
    std::vector<BlockOverride> overrides;

    // Index by packed chunk key for O(1) per-chunk lookup.
    using CK = long long;
    std::unordered_map<CK, std::vector<BlockOverride>> byChunk;

    static CK ck(int cx, int cz) {
        return ((long long)cx << 32) | (unsigned int)cz;
    }
    static int worldToCX(int wx) { return (int)floorf((float)wx / CW); }
    static int worldToCZ(int wz) { return (int)floorf((float)wz / CW); }

    // Add one block modification (village stamp or player edit).
    void addOverride(int wx, int wy, int wz, uint8_t block) {
        BlockOverride bo{wx, wy, wz, block};
        overrides.push_back(bo);
        byChunk[ck(worldToCX(wx), worldToCZ(wz))].push_back(bo);
    }

    // Return the override list for one chunk (empty list if none).
    const std::vector<BlockOverride>& forChunk(int cx, int cz) const {
        static const std::vector<BlockOverride> empty;
        auto it = byChunk.find(ck(cx, cz));
        return (it != byChunk.end()) ? it->second : empty;
    }

    bool load(const char* path) {
        FILE* f = fopen(path, "rb");
        if (!f) return false;
        uint32_t magic = 0, ver = 0;
        if (fread(&magic, 4, 1, f) != 1 || fread(&ver, 4, 1, f) != 1 ||
            magic != MAGIC || ver != VERSION) { fclose(f); return false; }
        uint8_t vp = 0;
        fread(&vp,                 1, 1, f);
        fread(&villageOriginX,     4, 1, f);
        fread(&villageOriginZ,     4, 1, f);
        fread(&villageBlocksOut,   4, 1, f);
        villagePlaced = (vp != 0);
        uint32_t cnt = 0; fread(&cnt, 4, 1, f);
        overrides.clear(); byChunk.clear();
        overrides.reserve(cnt);
        for (uint32_t i = 0; i < cnt; i++) {
            BlockOverride bo; uint8_t pad[3];
            fread(&bo.wx,    4, 1, f);
            fread(&bo.wy,    4, 1, f);
            fread(&bo.wz,    4, 1, f);
            fread(&bo.block, 1, 1, f);
            fread(pad,       3, 1, f);
            overrides.push_back(bo);
            byChunk[ck(worldToCX(bo.wx), worldToCZ(bo.wz))].push_back(bo);
        }
        fclose(f);
        return true;
    }

    bool save(const char* path) const {
        FILE* f = fopen(path, "wb");
        if (!f) return false;
        fwrite(&MAGIC,            4, 1, f);
        fwrite(&VERSION,          4, 1, f);
        uint8_t vp = villagePlaced ? 1u : 0u;
        fwrite(&vp,               1, 1, f);
        fwrite(&villageOriginX,   4, 1, f);
        fwrite(&villageOriginZ,   4, 1, f);
        fwrite(&villageBlocksOut, 4, 1, f);
        uint32_t cnt = (uint32_t)overrides.size();
        fwrite(&cnt, 4, 1, f);
        for (const auto& bo : overrides) {
            const uint8_t pad[3] = {0, 0, 0};
            fwrite(&bo.wx,    4, 1, f);
            fwrite(&bo.wy,    4, 1, f);
            fwrite(&bo.wz,    4, 1, f);
            fwrite(&bo.block, 1, 1, f);
            fwrite(pad,       3, 1, f);
        }
        fclose(f);
        return true;
    }
};
