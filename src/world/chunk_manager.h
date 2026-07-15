#pragma once
#include "chunk.h"
#include "biome.h"
#include "frustum.h"
#include "world_save.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <cstdint>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <vector>

static constexpr int VIEW_DISTANCE = 4; // chunks in each direction

struct ChunkKey {
    int cx, cz;
    bool operator==(const ChunkKey& o) const { return cx == o.cx && cz == o.cz; }
};

struct ChunkKeyHash {
    size_t operator()(const ChunkKey& k) const {
        return std::hash<long long>()(((long long)k.cx << 32) |
                                      (unsigned int)k.cz);
    }
};

class ChunkManager {
public:
    explicit ChunkManager(unsigned seed);
    ~ChunkManager();

    // Call every frame: loads/unloads chunks, rebuilds dirty meshes.
    void update(float playerX, float playerZ, const MeshAttribs& attr);

    // Draw chunks that pass the frustum test (shader active, uMVP uploaded).
    void render(const Frustum& fr);

    // World-space block operations. Return false if chunk not loaded.
    bool    setBlock(float wx, float wy, float wz, uint8_t type);
    uint8_t getBlock(float wx, float wy, float wz) const;

    int loadedChunkCount() const { return (int)chunks.size(); }
    int drawnChunkCount()  const { return statDrawn;  }
    int culledChunkCount() const { return statCulled; }
    int drawnIndexCount()  const { return statIndices; }

    // Lighting debug stats across all loaded chunks.
    float totalLightMs()   const;   // sum of lastLightMs for every loaded chunk
    void  lightStats(int& litBlocks, int& darkBlocks) const; // lit>=8 vs dark<8

    // Reporting
    void    biomeHistogram(int out[]) const;   // out[BIOME_COUNT]
    long    totalBlocks()    const;            // non-air blocks across loaded chunks
    long    totalVertices()  const;            // mesh vertices across loaded chunks
    uint8_t biomeAtWorld(float wx, float wz) const;

    // Underground stats across loaded chunks
    int   maxTerrainHeight() const;            // tallest column anywhere loaded
    long  totalCaveRemoved() const;            // blocks carved by caves
    void  oreHistogram(long out[6]) const;     // per-ore totals (Ores::ORES order)
    int   totalStructures()  const;            // structure origins across loaded chunks
    float lastGenMillis()    const { return lastGenMs.load(); }
    int   pendingChunks()    const { return inFlight.load(); }

    // Starter-village info (valid after construction).
    bool  villageIsPlaced()  const { return ws.villagePlaced; }
    int   villageOriginX()   const { return ws.villageOriginX; }
    int   villageOriginZ()   const { return ws.villageOriginZ; }
    int   villageBlocks()    const { return ws.villageBlocksOut; }
    int   saveOverrideCount()const { return (int)ws.overrides.size(); }
    // Fill (spawnX, spawnY, spawnZ) with a safe stand-in position above the well.
    void  getVillageCoords(float& spawnX, float& spawnY, float& spawnZ) const;

    // Spiral-search outward from origin for a column of biome `b`. On success
    // fills a safe spawn position (a few blocks above the surface) and returns
    // true. Used by the in-game spawn selector to jump between biomes.
    bool findBiomeSpawn(uint8_t biome, float& outX, float& outY, float& outZ) const;

    // Search the currently loaded chunks for a roofed underground air pocket (a
    // cave) with a solid floor, nearest to (px,pz). Fills a stand-in-cave
    // position. Lets the player jump straight into caves to test them.
    bool findCaveSpawn(float px, float pz, float& outX, float& outY, float& outZ) const;

private:
    unsigned seed;
    // Loaded chunks — MAIN THREAD ONLY (owns GL resources).
    std::unordered_map<ChunkKey, std::unique_ptr<Chunk>, ChunkKeyHash> chunks;

    int statDrawn = 0, statCulled = 0, statIndices = 0;
    std::atomic<float> lastGenMs{0.0f};  // cost of the most recent worker gen

    // ── Background generation ────────────────────────────────────────────────
    // The worker runs the pure-CPU TerrainGen::generate (no GL) off the main
    // thread; finished chunks return via readyQueue and are meshed on the main
    // thread, throttled per frame so neither generation nor upload hitches.
    // Each generation request carries the chunk's overrides from the world save
    // so the worker thread never touches WorldSave after construction.
    struct GenRequest {
        ChunkKey key;
        std::vector<BlockOverride> overrides;
    };

    WorldSave                          ws;              // world persistence (main thread)
    std::thread                        worker;
    std::mutex                         qmutex;
    std::condition_variable            qcv;
    std::deque<GenRequest>             requestQueue;    // coords+overrides (near→far)
    std::deque<std::unique_ptr<Chunk>> readyQueue;      // generated, awaiting mesh
    std::unordered_set<ChunkKey, ChunkKeyHash> pending; // requested/in-flight (main)
    std::atomic<bool>                  stopWorker{false};
    std::atomic<int>                   inFlight{0};     // requests not yet meshed

    void workerLoop();

    Chunk*        getChunkAt(int cx, int cz) const;
    ChunkNeighbors getNeighbors(int cx, int cz) const;
};
