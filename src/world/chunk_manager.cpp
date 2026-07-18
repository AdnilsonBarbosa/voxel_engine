#include "chunk_manager.h"
#include "fluid_interaction.h"
#include "terrain_gen.h"
#include "ore_gen.h"
#include "starter_village.h"
#include "lighting.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <algorithm>

static constexpr const char* DEFAULT_SAVE_PATH = "world.sav";
static const char* savePath() {
    const char* custom = SDL_getenv("VOXEL_SAVE");
    return (custom && *custom) ? custom : DEFAULT_SAVE_PATH;
}
static bool starterVillageEnabled() {
    const char* flag = SDL_getenv("VOXEL_VILLAGE");
    return flag && SDL_atoi(flag) != 0;
}
static bool villageSiteIsValid(const WorldSave& save, unsigned seed) {
    if (!save.villagePlaced) return false;
    const int wx = save.villageOriginX + 21;
    const int wz = save.villageOriginZ + 21;
    const int h = WorldGen::terrainHeight((float)wx, (float)wz, seed);
    return h > WATER_LEVEL + 3 &&
           Biomes::biomeAt((float)wx, (float)wz, h, seed) == Biomes::BIOME_PLAINS;
}

// Meshing (GL upload) happens on the main thread; cap per frame so a burst of
// finished chunks never stalls a frame with buffer uploads.
static constexpr int MAX_MESH_PER_FRAME = 2;

ChunkManager::ChunkManager(unsigned seed) : seed(seed) {
    // ── Load or create world save ────────────────────────────────────────────
    // Map pass: keep the save on disk, but do not apply its old village or
    // edits while the terrain composition is being evaluated. Set
    // VOXEL_VILLAGE=1 to restore the previous village workflow.
    if (!starterVillageEnabled()) {
        SDL_Log("[Village] Disabled for map pass; ignoring saved village overrides");
    } else if (ws.load(savePath())) {
        SDL_Log("[Village] Save loaded: village at (%d, %d), %d blocks, %d overrides in %d chunks",
                ws.villageOriginX, ws.villageOriginZ,
                ws.villageBlocksOut, (int)ws.overrides.size(), (int)ws.byChunk.size());

        // A saved village can outlive a terrain-generation change. Do not let
        // an old ocean coordinate become the player's new spawn point.
        if (!villageSiteIsValid(ws, seed)) {
            int ox = 0, oz = 0;
            if (StarterVillage::findSpawn(seed, ox, oz)) {
                const int blockCount = StarterVillage::build(ox, oz, seed,
                    [&](int wx, int wy, int wz, uint8_t block) {
                        ws.addOverride(wx, wy, wz, block);
                    });
                ws.villagePlaced    = true;
                ws.villageOriginX   = ox;
                ws.villageOriginZ   = oz;
                ws.villageBlocksOut = blockCount;
                ws.save(savePath());
                SDL_Log("[Village] Saved site was invalid; moved to dry plains at (%d, %d): %d blocks",
                        ox, oz, blockCount);
            } else {
                SDL_Log("[Village] WARNING: saved site is invalid and no dry plains spawn was found");
            }
        }
    } else {
        // First run — find a flat plains area and build the starter village.
        SDL_Log("[Village] New world — searching for village spawn...");
        auto t0 = SDL_GetTicks();
        int ox = 0, oz = 0;
        if (StarterVillage::findSpawn(seed, ox, oz)) {
            int blockCount = StarterVillage::build(ox, oz, seed,
                [&](int wx, int wy, int wz, uint8_t block) {
                    ws.addOverride(wx, wy, wz, block);
                });
            ws.villagePlaced    = true;
            ws.villageOriginX   = ox;
            ws.villageOriginZ   = oz;
            ws.villageBlocksOut = blockCount;
            ws.save(savePath());
            auto t1 = SDL_GetTicks();
            SDL_Log("[Village] Built at (%d, %d): %d blocks, %d overrides, %d chunks affected, %ums",
                    ox, oz, blockCount, (int)ws.overrides.size(),
                    (int)ws.byChunk.size(), (unsigned)(t1 - t0));
        } else {
            SDL_Log("[Village] WARNING: No flat plains found within 1200 blocks of origin.");
        }
    }

    worker = std::thread(&ChunkManager::workerLoop, this);
}

ChunkManager::~ChunkManager() {
    if (savePending) ws.save(savePath());   // flush edits still in the debounce window
    stopWorker.store(true);
    qcv.notify_all();
    if (worker.joinable()) worker.join();
}

// Background thread: pulls GenRequests, runs terrain generation, applies world-
// save overrides (village + player edits), and returns finished chunks.
void ChunkManager::workerLoop() {
    for (;;) {
        GenRequest req;
        {
            std::unique_lock<std::mutex> lk(qmutex);
            qcv.wait(lk, [&]{ return stopWorker.load() || !requestQueue.empty(); });
            if (stopWorker.load()) return;
            req = std::move(requestQueue.front());
            requestQueue.pop_front();
        }

        const ChunkKey& key = req.key;
        auto c = std::make_unique<Chunk>(key.cx, key.cz);
        TerrainGen::GenStats gs;
        auto t0 = std::chrono::high_resolution_clock::now();
        TerrainGen::generate(c->blocks, key.cx, key.cz, seed,
                             &c->dominantBiome, &c->blockCount, &gs);

        // Apply world-save overrides: village blocks + player modifications.
        // req.overrides was extracted from ws.byChunk on the main thread before
        // this request was queued, so no lock is needed here.
        for (const auto& bo : req.overrides) {
            int lx = bo.wx - key.cx * CHUNK_W;
            int lz = bo.wz - key.cz * CHUNK_D;
            if (lx >= 0 && lx < CHUNK_W && lz >= 0 && lz < CHUNK_D &&
                bo.wy >= 1 && bo.wy < CHUNK_H) {
                c->blocks[lx][bo.wy][lz] = bo.block;
            }
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        lastGenMs.store(std::chrono::duration<float, std::milli>(t1 - t0).count());

        c->maxHeight   = gs.maxHeight;
        c->caveRemoved = gs.caveRemoved;
        for (int i = 0; i < Ores::ORE_COUNT; i++)
            c->oreCounts[i] = gs.oreCounts[i];
        c->structureCount = gs.structures;
        c->structureType  = gs.structureType;
        if (gs.structures > 0)
            SDL_Log("[Structure] %d x %s in chunk(%d,%d) biome=%s",
                    gs.structures, Structures::def((Structures::Type)gs.structureType).name,
                    key.cx, key.cz, Biomes::info((Biomes::Biome)c->dominantBiome).name);

        {
            std::lock_guard<std::mutex> lk(qmutex);
            readyQueue.push_back(std::move(c));
        }
    }
}

Chunk* ChunkManager::getChunkAt(int cx, int cz) const {
    auto it = chunks.find({cx, cz});
    return (it != chunks.end()) ? it->second.get() : nullptr;
}

ChunkNeighbors ChunkManager::getNeighbors(int cx, int cz) const {
    ChunkNeighbors nb;
    nb.px = getChunkAt(cx + 1, cz);
    nb.nx = getChunkAt(cx - 1, cz);
    nb.pz = getChunkAt(cx, cz + 1);
    nb.nz = getChunkAt(cx, cz - 1);
    return nb;
}

void ChunkManager::update(float playerX, float playerZ, const MeshAttribs& attr) {
    int pcx = (int)floorf(playerX / CHUNK_W);
    int pcz = (int)floorf(playerZ / CHUNK_D);

    // ── 1. Unload chunks outside range; assign LOD ring inside it ────────────
    // Chunks in the outer rings mesh simplified (no cross-plants, flat AO);
    // crossing the ring boundary re-queues a rebuild via the dirty flag.
    const int LOD_FAR_DIST = viewDistance_ > 3 ? viewDistance_ - 1 : viewDistance_;
    for (auto it = chunks.begin(); it != chunks.end(); ) {
        int dx = it->first.cx - pcx;
        int dz = it->first.cz - pcz;
        if (abs(dx) > viewDistance_ + 1 || abs(dz) > viewDistance_ + 1) {
            it = chunks.erase(it);
            continue;
        }
        const int cheb = abs(dx) > abs(dz) ? abs(dx) : abs(dz);
        const int lod = cheb >= LOD_FAR_DIST ? 1 : 0;
        if (it->second->lodLevel != lod) {
            it->second->lodLevel = lod;
            it->second->dirty = true;
        }
        ++it;
    }

    // ── 2. Integrate chunks the worker finished (cheap: pointer moves) ───────
    std::deque<std::unique_ptr<Chunk>> done;
    {
        std::lock_guard<std::mutex> lk(qmutex);
        done.swap(readyQueue);
    }
    for (auto& c : done) {
        ChunkKey key{ c->cx, c->cz };
        pending.erase(key);
        inFlight.fetch_sub(1);

        int dx = key.cx - pcx, dz = key.cz - pcz;
        if (abs(dx) > viewDistance_ + 1 || abs(dz) > viewDistance_ + 1)
            continue;                       // player moved away — discard
        if (chunks.count(key)) continue;    // safety: already present

        chunks[key] = std::move(c);
        {
            const int cheb = abs(dx) > abs(dz) ? abs(dx) : abs(dz);
            chunks[key]->lodLevel = cheb >= LOD_FAR_DIST ? 1 : 0;
        }
        chunks[key]->dirty = true;
        // Existing neighbors must re-cull their shared border faces
        if (Chunk* n = getChunkAt(key.cx+1, key.cz)) n->dirty = true;
        if (Chunk* n = getChunkAt(key.cx-1, key.cz)) n->dirty = true;
        if (Chunk* n = getChunkAt(key.cx, key.cz+1)) n->dirty = true;
        if (Chunk* n = getChunkAt(key.cx, key.cz-1)) n->dirty = true;
    }

    // ── 3. Queue missing in-range chunks, nearest first ─────────────────────
    std::vector<ChunkKey> needed;
    for (int dz = -viewDistance_; dz <= viewDistance_; dz++)
        for (int dx = -viewDistance_; dx <= viewDistance_; dx++) {
            ChunkKey key{ pcx + dx, pcz + dz };
            if (chunks.count(key) || pending.count(key)) continue;
            needed.push_back(key);
        }
    if (!needed.empty()) {
        std::sort(needed.begin(), needed.end(), [&](const ChunkKey& a, const ChunkKey& b){
            int da = (a.cx-pcx)*(a.cx-pcx) + (a.cz-pcz)*(a.cz-pcz);
            int db = (b.cx-pcx)*(b.cx-pcx) + (b.cz-pcz)*(b.cz-pcz);
            return da < db;
        });
        {
            std::lock_guard<std::mutex> lk(qmutex);
            for (const ChunkKey& key : needed) {
                pending.insert(key);
                inFlight.fetch_add(1);
                GenRequest req;
                req.key      = key;
                req.overrides = ws.forChunk(key.cx, key.cz);   // copy for worker
                requestQueue.push_back(std::move(req));
            }
        }
        qcv.notify_all();
    }

    // ── 4. Rebuild dirty meshes on the main thread (throttled) ──────────────
    int built = 0;
    for (auto& [key, chunk] : chunks) {
        if (!chunk->dirty) continue;
        ChunkNeighbors nb = getNeighbors(key.cx, key.cz);
        chunk->buildMesh(attr, nb);
        if (++built >= MAX_MESH_PER_FRAME) break;
    }

    // ── 5. Debounced world save: one write after building settles ───────────
    if (savePending && SDL_GetTicks() - lastEditMs > 1500u) {
        ws.save(savePath());
        savePending = false;
    }
}

void ChunkManager::render(const Frustum& fr) {
    statDrawn = statCulled = statIndices = 0;
    for (auto& [key, chunk] : chunks) {
        float minx = chunk->worldX(), minz = chunk->worldZ();
        if (!fr.aabbVisible(minx, 0.0f, minz,
                            minx + CHUNK_W, (float)CHUNK_H, minz + CHUNK_D)) {
            statCulled++;
            continue;
        }
        chunk->render();
        statDrawn++;
        statIndices += chunk->indexCount();
    }
}

bool ChunkManager::setBlock(float wx, float wy, float wz, uint8_t type) {
    const int bx = (int)floorf(wx);
    const int by = (int)floorf(wy);
    const int bz = (int)floorf(wz);
    // Keep bedrock and the outside of the finite world immutable. Checking
    // before touching the chunk also prevents invalid edits from being saved.
    if (by <= 0 || by >= CHUNK_H || type >= BLOCK_COUNT)
        return false;

    int cx = (int)floorf((float)bx / CHUNK_W);
    int cz = (int)floorf((float)bz / CHUNK_D);
    Chunk* c = getChunkAt(cx, cz);
    if (!c) return false;

    int lx = bx - cx * CHUNK_W;
    int ly = by;
    int lz = bz - cz * CHUNK_D;
    if (lx < 0 || lx >= CHUNK_W || lz < 0 || lz >= CHUNK_D)
        return false;
    c->setBlock(lx, ly, lz, type);

    // Persist player edit so the block survives chunk unload/reload. The
    // actual file write is debounced in update() — writing the whole save
    // per placed block caused a visible hitch while building.
    ws.addOverride(bx, ly, bz, type);
    savePending = true;
    lastEditMs = SDL_GetTicks();

    // Mark neighbors dirty when the modified block is on a chunk border
    if (lx == 0)           { Chunk* n = getChunkAt(cx-1, cz); if (n) n->dirty = true; }
    if (lx == CHUNK_W - 1) { Chunk* n = getChunkAt(cx+1, cz); if (n) n->dirty = true; }
    if (lz == 0)           { Chunk* n = getChunkAt(cx, cz-1); if (n) n->dirty = true; }
    if (lz == CHUNK_D - 1) { Chunk* n = getChunkAt(cx, cz+1); if (n) n->dirty = true; }

    // Fluid contact rules (water hardens lava) — touched cells only.
    if (type != BLOCK_STONE)   // results don't re-trigger scans of themselves
        Fluids::applyAt(*this, bx, ly, bz);
    return true;
}

bool ChunkManager::isLoadedAt(int wx, int wy, int wz) const {
    if (wy < 0 || wy >= CHUNK_H) return false;
    int cx = (int)floorf((float)wx / CHUNK_W);
    int cz = (int)floorf((float)wz / CHUNK_D);
    return getChunkAt(cx, cz) != nullptr;
}

void ChunkManager::biomeHistogram(int out[]) const {
    for (int i = 0; i < Biomes::BIOME_COUNT; i++) out[i] = 0;
    for (auto& [key, chunk] : chunks) out[chunk->dominantBiome]++;
}

long ChunkManager::totalBlocks() const {
    long n = 0;
    for (auto& [key, chunk] : chunks) n += chunk->blockCount;
    return n;
}

long ChunkManager::totalVertices() const {
    long n = 0;
    for (auto& [key, chunk] : chunks) n += chunk->vertexCount();
    return n;
}

int ChunkManager::totalStructures() const {
    int n = 0;
    for (auto& [key, chunk] : chunks) n += chunk->structureCount;
    return n;
}

int ChunkManager::maxTerrainHeight() const {
    int m = 0;
    for (auto& [key, chunk] : chunks) if (chunk->maxHeight > m) m = chunk->maxHeight;
    return m;
}

long ChunkManager::totalCaveRemoved() const {
    long n = 0;
    for (auto& [key, chunk] : chunks) n += chunk->caveRemoved;
    return n;
}

void ChunkManager::oreHistogram(long out[6]) const {
    for (int i = 0; i < 6; i++) out[i] = 0;
    for (auto& [key, chunk] : chunks)
        for (int i = 0; i < 6; i++) out[i] += chunk->oreCounts[i];
}

uint8_t ChunkManager::biomeAtWorld(float wx, float wz) const {
    int cx = (int)floorf(wx / CHUNK_W);
    int cz = (int)floorf(wz / CHUNK_D);
    Chunk* c = getChunkAt(cx, cz);
    return c ? c->dominantBiome : (uint8_t)Biomes::BIOME_PLAINS;
}

bool ChunkManager::findBiomeSpawn(uint8_t biome, float& outX, float& outY, float& outZ) const {
    // Sample columns on an outward-growing square ring until one classifies as
    // the requested biome. Step of 8 keeps the search cheap; range covers the
    // ~250-block biome regions many times over.
    const int STEP = 8;
    const WorldGen::WorldGenerationConfig cfg = WorldGen::config(seed);
    // Pass 0: a clearing near the origin (camera never inside a canopy).
    // Pass 1: any column of the biome — never strands the player on a far
    // island just because the home forest is dense.
    for (int pass = 0; pass < 2; pass++) {
        const int maxR = pass == 0 ? 320 : 4000;
        for (int r = 0; r <= maxR; r += STEP) {
            for (int dz = -r; dz <= r; dz += STEP) {
                for (int dx = -r; dx <= r; dx += STEP) {
                    // Only the perimeter of the ring (interior already checked)
                    if (r > 0 && abs(dx) != r && abs(dz) != r) continue;
                    float wx = (float)dx, wz = (float)dz;
                    int h = TerrainGen::columnHeight(wx, wz, seed);
                    if (Biomes::biomeAt(wx, wz, h, seed) != biome) continue;
                    if (pass == 0) {
                        bool clear = true;
                        for (int tz = dz - 3; tz <= dz + 3 && clear; tz++)
                        for (int tx = dx - 3; tx <= dx + 3 && clear; tx++) {
                            const int th = TerrainGen::columnHeight((float)tx, (float)tz, seed);
                            const Biomes::Biome tb = Biomes::biomeAt((float)tx, (float)tz, th, seed);
                            if (Decoration::wouldPlaceTree(tx, tz, th, tb, seed, cfg))
                                clear = false;
                        }
                        if (!clear) continue;
                    }
                    outX = wx + 0.5f;
                    outZ = wz + 0.5f;
                    outY = (float)TerrainGen::surfaceHeight(wx, wz, seed) + 3.0f;
                    return true;
                }
            }
        }
    }
    return false;
}

bool ChunkManager::findCaveSpawn(float px, float pz, float& outX, float& outY, float& outZ) const {
    bool  found = false;
    long  bestDist = 0x7FFFFFFFL;
    for (auto& [key, chunk] : chunks) {
        for (int y = 14; y < 82; y++)
        for (int lx = 0; lx < CHUNK_W; lx++)
        for (int lz = 0; lz < CHUNK_D; lz++) {
            if (chunk->getBlock(lx, y,   lz) != BLOCK_AIR) continue; // void
            if (chunk->getBlock(lx, y+1, lz) != BLOCK_AIR) continue; // head room
            if (!blockIsOpaque(chunk->getBlock(lx, y-1, lz))) continue; // solid floor
            // Must be roofed (a real cave, not open sky above terrain)
            bool roof = false;
            for (int r = 2; r <= 12; r++)
                if (blockIsOpaque(chunk->getBlock(lx, y+r, lz))) { roof = true; break; }
            if (!roof) continue;

            int wx = key.cx * CHUNK_W + lx;
            int wz = key.cz * CHUNK_D + lz;
            long dx = (long)(wx - px), dz = (long)(wz - pz);
            long d  = dx*dx + dz*dz;
            if (d < bestDist) {
                bestDist = d;
                outX = wx + 0.5f; outY = (float)y + 1.2f; outZ = wz + 0.5f;
                found = true;
            }
        }
    }
    return found;
}

uint8_t ChunkManager::getBlock(float wx, float wy, float wz) const {
    int cx = (int)floorf(wx / CHUNK_W);
    int cz = (int)floorf(wz / CHUNK_D);
    Chunk* c = getChunkAt(cx, cz);
    if (!c) return BLOCK_AIR;
    int lx = (int)floorf(wx) - cx * CHUNK_W;
    int ly = (int)floorf(wy);
    int lz = (int)floorf(wz) - cz * CHUNK_D;
    return c->getBlock(lx, ly, lz);
}

float ChunkManager::totalLightMs() const {
    float s = 0.0f;
    for (auto& [key, chunk] : chunks) s += chunk->lastLightMs;
    return s;
}

void ChunkManager::lightStats(int& litBlocks, int& darkBlocks) const {
    litBlocks = darkBlocks = 0;
    for (auto& [key, chunk] : chunks) {
        int lit = 0, dark = 0;
        Lighting::countStats(*chunk, lit, dark);
        litBlocks  += lit;
        darkBlocks += dark;
    }
}

void ChunkManager::getVillageCoords(float& spawnX, float& spawnY, float& spawnZ) const {
    // Stand at the well centre (19 blocks east + 21 north of origin), 4 blocks above surface.
    float cx = (float)(ws.villageOriginX + 21) + 0.5f;
    float cz = (float)(ws.villageOriginZ + 21) + 0.5f;
    spawnX = cx;
    spawnZ = cz;
    spawnY = (float)TerrainGen::surfaceHeight(cx, cz, seed) + 6.0f;
}
