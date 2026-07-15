#pragma once
#include "gl_ext.h"
#include "block_types.h"
#include <cstdint>

static constexpr int CHUNK_W = 16;
static constexpr int CHUNK_H = 128;
static constexpr int CHUNK_D = 16;

class Chunk;

struct ChunkNeighbors {
    const Chunk* px = nullptr; // +X neighbor
    const Chunk* nx = nullptr; // -X neighbor
    const Chunk* pz = nullptr; // +Z neighbor
    const Chunk* nz = nullptr; // -Z neighbor
};

// Shader attribute locations for the textured vertex format
// [ pos.xyz | uv.xy | tileOrigin.xy | light | sky | block | wave ] = 11 floats.
struct MeshAttribs {
    GLint pos = -1, uv = -1, tile = -1, light = -1, sky = -1, block = -1, wave = -1;
};

class Chunk {
public:
    int  cx, cz;        // chunk-space coordinates (world / 16)
    bool dirty = true;  // mesh needs rebuild

    uint8_t dominantBiome = 0;  // most common biome in this chunk
    long    blockCount    = 0;  // non-air blocks generated

    // Underground generation stats (filled by TerrainGen::generate)
    int  maxHeight   = 0;       // tallest terrain column
    long caveRemoved = 0;       // blocks carved out by caves
    long oreCounts[6] = {0};    // per-ore block counts (order = Ores::ORES)

    // Structure stats
    int     structureCount = 0; // structure origins in this chunk
    uint8_t structureType  = 0; // last structure type placed (Structures::Type)

    uint8_t blocks    [CHUNK_W][CHUNK_H][CHUNK_D]; // [x][y][z]
    uint8_t lightSky  [CHUNK_W][CHUNK_H][CHUNK_D]; // 0-15  sunlight (computed in buildMesh)
    uint8_t lightBlock[CHUNK_W][CHUNK_H][CHUNK_D]; // 0-15  block light (torches etc.)
    bool    lightValid  = false; // true once buildMesh has run; neighbors seed from it
    float   lastLightMs = 0.0f; // cost of the most recent lighting computation (ms)

    Chunk(int cx, int cz);
    ~Chunk();

    uint8_t getBlock(int x, int y, int z) const;
    void    setBlock(int x, int y, int z, uint8_t type);

    // Build / rebuild the textured mesh (greedy for solids, per-block for water/plants).
    void buildMesh(const MeshAttribs& attr, const ChunkNeighbors& nb);

    // Draw — shader must already be active with MVP set.
    void render() const;

    void cleanup();

    float worldX() const;
    float worldZ() const;
    int   indexCount()  const { return idxCount; }
    int   vertexCount() const { return vertCount; }

private:
    GLuint vao=0, vbo=0, ibo=0;
    int    idxCount=0;
    int    vertCount=0;
};
