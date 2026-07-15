#include "chunk.h"
#include "block_types.h"
#include "block_material.h"
#include "lighting.h"
#include <vector>
#include <cstring>
#include <cstdint>
#include <chrono>

// Face definitions: integer normal + 4 quad vertices (block-local, 0/1 coords)
struct FaceDef { int n[3]; float v[4][3]; };
static const FaceDef FACES[6] = {
    { { 0, 1, 0}, { {0,1,0},{1,1,0},{1,1,1},{0,1,1} } }, // +Y top
    { { 0,-1, 0}, { {0,0,1},{1,0,1},{1,0,0},{0,0,0} } }, // -Y bottom
    { { 1, 0, 0}, { {1,0,0},{1,1,0},{1,1,1},{1,0,1} } }, // +X right
    { {-1, 0, 0}, { {0,0,1},{0,1,1},{0,1,0},{0,0,0} } }, // -X left
    { { 0, 0, 1}, { {0,0,1},{1,0,1},{1,1,1},{0,1,1} } }, // +Z front
    { { 0, 0,-1}, { {1,0,0},{0,0,0},{0,1,0},{1,1,0} } }, // -Z back
};

// Ambient-occlusion brightness for occlusion levels 0..3 (0 = most occluded)
static const float AO_LUT[4] = { 0.45f, 0.65f, 0.82f, 1.00f };

// Deterministic hash for seed-driven texture variation.
static inline uint32_t chash(int x, int y, int z) {
    uint32_t h = (uint32_t)(x*73856093) ^ (uint32_t)(y*19349663) ^ (uint32_t)(z*83492791);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return h;
}

// Top-left UV of an atlas tile slot.
static inline void tileUV(uint16_t tile, float& tox, float& toy) {
    tox = (float)((tile % Atlas::ATLAS_COLS) * Atlas::TILE_PX) / (float)Atlas::ATLAS_PX;
    toy = (float)((tile / Atlas::ATLAS_COLS) * Atlas::TILE_PX) / (float)Atlas::ATLAS_PX;
}

Chunk::Chunk(int cx, int cz) : cx(cx), cz(cz), dirty(true) {
    memset(blocks,     0, sizeof(blocks));
    memset(lightSky,   0, sizeof(lightSky));
    memset(lightBlock, 0, sizeof(lightBlock));
}

Chunk::~Chunk() { cleanup(); }

uint8_t Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_W || y < 0 || y >= CHUNK_H || z < 0 || z >= CHUNK_D)
        return BLOCK_AIR;
    return blocks[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, uint8_t type) {
    if (x < 0 || x >= CHUNK_W || y < 0 || y >= CHUNK_H || z < 0 || z >= CHUNK_D)
        return;
    blocks[x][y][z] = type;
    dirty = true;
}

float Chunk::worldX() const { return (float)(cx * CHUNK_W); }
float Chunk::worldZ() const { return (float)(cz * CHUNK_D); }

void Chunk::buildMesh(const MeshAttribs& attr, const ChunkNeighbors& nb) {
    std::vector<float>        verts;
    std::vector<unsigned int> indices;
    verts.reserve(1 << 16);
    indices.reserve(1 << 16);

    const float wx0 = worldX();
    const float wz0 = worldZ();
    const int   iwx0 = cx * CHUNK_W;
    const int   iwz0 = cz * CHUNK_D;

    // Neighbor-aware block sampler. Diagonal chunks are unavailable, so a
    // sample that is out of range in BOTH x and z is treated as air.
    auto blockAt = [&](int lx, int ly, int lz) -> uint8_t {
        if (ly < 0 || ly >= CHUNK_H) return BLOCK_AIR;
        bool xo = (lx < 0 || lx >= CHUNK_W);
        bool zo = (lz < 0 || lz >= CHUNK_D);
        if (xo && zo) return BLOCK_AIR;
        const Chunk* c = this;
        if      (lx < 0)         { c = nb.nx; lx += CHUNK_W; }
        else if (lx >= CHUNK_W)  { c = nb.px; lx -= CHUNK_W; }
        else if (lz < 0)         { c = nb.nz; lz += CHUNK_D; }
        else if (lz >= CHUNK_D)  { c = nb.pz; lz -= CHUNK_D; }
        if (!c) return BLOCK_AIR;
        return c->blocks[lx][ly][lz];
    };
    auto opaqueAt = [&](int x, int y, int z) { return blockIsOpaque(blockAt(x, y, z)); };

    // Textured vertex: [ pos.xyz | uv.xy | tileOrigin.xy | light | sky | block | wave ] (11 floats)
    auto pushV = [&](float px, float py, float pz, float u, float v,
                     float tox, float toy, float light, float sky, float block, float wave) {
        verts.push_back(px);  verts.push_back(py);  verts.push_back(pz);
        verts.push_back(u);   verts.push_back(v);
        verts.push_back(tox); verts.push_back(toy);
        verts.push_back(light); verts.push_back(sky); verts.push_back(block); verts.push_back(wave);
    };

    // Texture variant selected per 4x4x4 world region (variety without breaking
    // greedy merging within a region).
    auto regionVariant = [&](int wx, int wy, int wz) -> uint32_t {
        return chash(wx >> 2, wy >> 2, wz >> 2);
    };

    // ── Lighting (sky + block) — BFS with cross-chunk seeding ────────────────
    // LightingSystem computes both maps into chunk member arrays so neighbour
    // chunks can seed from them on their own rebuilds (cross-chunk propagation).
    auto t_lightStart = std::chrono::high_resolution_clock::now();
    Lighting::computeAll(*this, nb);
    auto t_lightEnd   = std::chrono::high_resolution_clock::now();
    lastLightMs = std::chrono::duration<float, std::milli>(t_lightEnd - t_lightStart).count();

    auto skyOf = [&](int x, int y, int z) -> float {
        return lightSky[x][y][z] * (1.0f / 15.0f);
    };
    auto blockOf = [&](int x, int y, int z) -> float {
        return lightBlock[x][y][z] * (1.0f / 14.0f);
    };
    auto blockFaceOf = [&](int x, int y, int z) -> float {
        if (x<0||x>=CHUNK_W||y<0||y>=CHUNK_H||z<0||z>=CHUNK_D) return 0.0f;
        return lightBlock[x][y][z] * (1.0f / 14.0f);
    };

    // Per-face-corner ambient occlusion levels (0..3) for the face `fi` of the
    // block at (x,y,z). Same classic 3-neighbour sampling as before.
    auto faceAO = [&](int fi, int x, int y, int z, int outAO[4]) {
        const FaceDef& fd = FACES[fi];
        for (int vi = 0; vi < 4; vi++) {
            int cv[3] = { (int)fd.v[vi][0], (int)fd.v[vi][1], (int)fd.v[vi][2] };
            int taxis[2], tsign[2], k = 0;
            for (int a = 0; a < 3; a++)
                if (fd.n[a] == 0) { taxis[k] = a; tsign[k] = cv[a]*2 - 1; k++; }
            int o1[3] = { fd.n[0], fd.n[1], fd.n[2] };
            int o2[3] = { fd.n[0], fd.n[1], fd.n[2] };
            int oc[3] = { fd.n[0], fd.n[1], fd.n[2] };
            o1[taxis[0]] += tsign[0];
            o2[taxis[1]] += tsign[1];
            oc[taxis[0]] += tsign[0]; oc[taxis[1]] += tsign[1];
            bool s1 = opaqueAt(x+o1[0], y+o1[1], z+o1[2]);
            bool s2 = opaqueAt(x+o2[0], y+o2[1], z+o2[2]);
            bool sc = opaqueAt(x+oc[0], y+oc[1], z+oc[2]);
            outAO[vi] = (s1 && s2) ? 0 : 3 - ((int)s1 + (int)s2 + (int)sc);
        }
    };

    // ── Water + cross-plants: per-block emit (not greedy-meshed) ─────────────
    for (int x = 0; x < CHUNK_W; x++)
    for (int y = 0; y < CHUNK_H; y++)
    for (int z = 0; z < CHUNK_D; z++) {
        uint8_t bt = blocks[x][y][z];
        if (bt == BLOCK_AIR) continue;
        RenderKind kind = blockRenderKind(bt);
        if (kind == RK_SOLID) continue;              // handled by greedy pass

        float skyB = skyOf(x, y, z);
        float blkB = blockOf(x, y, z);

        if (kind == RK_CROSS) {
            uint16_t tile = Materials::tileForFace(bt, 2, 0); // side tile
            float tox, toy; tileUV(tile, tox, toy);
            static const float CROSS[2][4][3] = {
                { {0,0,0},{1,0,1},{1,1,1},{0,1,0} },
                { {0,0,1},{1,0,0},{1,1,0},{0,1,1} },
            };
            for (int q = 0; q < 2; q++) {
                unsigned int base = (unsigned int)(verts.size() / 11);
                for (int vi = 0; vi < 4; vi++) {
                    float ox = CROSS[q][vi][0], oy = CROSS[q][vi][1], oz = CROSS[q][vi][2];
                    float uu = (vi == 1 || vi == 2) ? 1.0f : 0.0f;
                    float vv = (oy > 0.5f) ? 0.0f : 1.0f;        // image top at plant top
                    float wave = (oy > 0.5f) ? 0.5f : 0.0f;      // sway the top
                    pushV(wx0 + x + ox, (float)y + oy, wz0 + z + oz,
                          uu, vv, tox, toy, 1.0f, skyB, blkB, wave);
                }
                indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
                indices.push_back(base+0); indices.push_back(base+2); indices.push_back(base+3);
            }
            continue;
        }

        // RK_LIQUID (water): animated top ring, no AO, show only against air/plants
        uint16_t wtile = Materials::tileForFace(bt, 0, 0);
        float wtox, wtoy; tileUV(wtile, wtox, wtoy);
        for (int fi = 0; fi < 6; fi++) {
            const FaceDef& fd = FACES[fi];
            uint8_t neigh = blockAt(x+fd.n[0], y+fd.n[1], z+fd.n[2]);
            if (neigh == BLOCK_WATER || blockIsOpaque(neigh)) continue;
            int ua = fd.n[0] ? 1 : 0;            // a tangent axis for uv
            int va = fd.n[2] ? 1 : 2;            // the other tangent axis
            float shade = FACE_SHADE[fi];
            unsigned int base = (unsigned int)(verts.size() / 11);
            for (int vi = 0; vi < 4; vi++) {
                float oy = fd.v[vi][1];
                float py = (oy > 0.5f) ? (float)y + 0.88f : (float)y;
                float wv = (oy > 0.5f) ? 1.0f : 0.0f;
                pushV(wx0 + x + fd.v[vi][0], py, wz0 + z + fd.v[vi][2],
                      fd.v[vi][ua], fd.v[vi][va], wtox, wtoy, shade, skyB, blkB, wv);
            }
            indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
            indices.push_back(base+0); indices.push_back(base+2); indices.push_back(base+3);
        }
    }

    // ── Greedy meshing for solid cubes ──────────────────────────────────────
    // For each of the 6 face directions, sweep slice-by-slice and merge coplanar
    // faces that share the SAME atlas tile and are UNIFORMLY lit (all 4 AO
    // corners equal) into large rectangles. Faces with an AO gradient stay 1x1
    // with full per-corner AO. The tile repeats across a merged quad (shader
    // wraps UVs), so the look is identical — just far fewer vertices.
    static uint8_t  mValid  [128][128];
    static uint8_t  mUsed   [128][128];
    static uint8_t  mUniform[128][128];
    static uint16_t mTile   [128][128];
    static uint8_t  mSky    [128][128];
    static uint8_t  mBlk    [128][128];
    static int8_t   mAO     [128][128][4];
    const int dim[3] = { CHUNK_W, CHUNK_H, CHUNK_D };

    auto emitQuad = [&](int fi, int nAxis, int u, int v, int s,
                        int i0, int j0, int w, int h, uint16_t tile, float sky, float block, const int ao4[4]) {
        const FaceDef& fd = FACES[fi];
        float shade = FACE_SHADE[fi];
        float tox, toy; tileUV(tile, tox, toy);
        float light[4];
        unsigned int base = (unsigned int)(verts.size() / 11);
        for (int vi = 0; vi < 4; vi++) {
            int p[3];
            p[nAxis] = s  + (int)fd.v[vi][nAxis];
            p[u]     = i0 + (int)fd.v[vi][u] * w;
            p[v]     = j0 + (int)fd.v[vi][v] * h;
            float uu = (float)((int)fd.v[vi][u] * w);   // 0 or w  → tile repeats w×
            float vv = (float)((int)fd.v[vi][v] * h);   // 0 or h  → tile repeats h×
            light[vi] = shade * AO_LUT[ao4[vi]];
            pushV(wx0 + (float)p[0], (float)p[1], wz0 + (float)p[2],
                  uu, vv, tox, toy, light[vi], sky, block, 0.0f);
        }
        if (light[0] + light[2] < light[1] + light[3]) {
            indices.push_back(base+1); indices.push_back(base+2); indices.push_back(base+3);
            indices.push_back(base+1); indices.push_back(base+3); indices.push_back(base+0);
        } else {
            indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
            indices.push_back(base+0); indices.push_back(base+2); indices.push_back(base+3);
        }
    };

    for (int fi = 0; fi < 6; fi++) {
        const FaceDef& fd = FACES[fi];
        int nAxis = fd.n[0] ? 0 : (fd.n[1] ? 1 : 2);
        int u = -1, v = -1;
        for (int a = 0; a < 3; a++) if (a != nAxis) { (u < 0 ? u : v) = a; }
        int dimN = dim[nAxis], dimU = dim[u], dimV = dim[v];

        for (int s = 0; s < dimN; s++) {
            // Build the face mask for this slice
            for (int i = 0; i < dimU; i++)
            for (int j = 0; j < dimV; j++) {
                mValid[i][j] = mUsed[i][j] = 0;
                int c[3]; c[nAxis] = s; c[u] = i; c[v] = j;
                uint8_t bt = blocks[c[0]][c[1]][c[2]];
                if (blockRenderKind(bt) != RK_SOLID) continue;
                if (blockIsOpaque(blockAt(c[0]+fd.n[0], c[1]+fd.n[1], c[2]+fd.n[2])))
                    continue;                             // face culled
                int ao[4]; faceAO(fi, c[0], c[1], c[2], ao);
                uint32_t variant = regionVariant(iwx0 + c[0], c[1], iwz0 + c[2]);
                mValid[i][j]   = 1;
                mTile[i][j]    = Materials::tileForFace(bt, fi, variant);
                // Sky and block light = light of the air cell this face looks at,
                // not the solid block itself (solid blocks always have lightSky=0).
                { int nx=c[0]+fd.n[0], ny=c[1]+fd.n[1], nz=c[2]+fd.n[2];
                  if (ny >= CHUNK_H) {
                      mSky[i][j] = 15; mBlk[i][j] = 0;           // above world → full sky
                  } else if (nx>=0&&nx<CHUNK_W&&ny>=0&&nz>=0&&nz<CHUNK_D) {
                      mSky[i][j] = lightSky[nx][ny][nz];          // within this chunk
                      mBlk[i][j] = lightBlock[nx][ny][nz];
                  } else {
                      // Adjacent air is in a neighbour chunk — read from it if available
                      const Chunk* nc = nullptr;
                      int lx=nx, lz=nz;
                      if      (nx < 0)        { nc = nb.nx; lx = nx + CHUNK_W; }
                      else if (nx >= CHUNK_W) { nc = nb.px; lx = nx - CHUNK_W; }
                      else if (nz < 0)        { nc = nb.nz; lz = nz + CHUNK_D; }
                      else if (nz >= CHUNK_D) { nc = nb.pz; lz = nz - CHUNK_D; }
                      if (nc && nc->lightValid && ny>=0) {
                          mSky[i][j] = nc->lightSky[lx][ny][lz];
                          mBlk[i][j] = nc->lightBlock[lx][ny][lz];
                      } else {
                          mSky[i][j] = 15; mBlk[i][j] = 0;       // neighbour not ready → assume open sky
                      }
                  } }
                mUniform[i][j] = (ao[0]==ao[1] && ao[1]==ao[2] && ao[2]==ao[3]) ? 1 : 0;
                for (int t = 0; t < 4; t++) mAO[i][j][t] = (int8_t)ao[t];
            }

            // Greedily extract rectangles
            for (int j = 0; j < dimV; j++)
            for (int i = 0; i < dimU; i++) {
                if (!mValid[i][j] || mUsed[i][j]) continue;

                int ao[4] = { mAO[i][j][0], mAO[i][j][1], mAO[i][j][2], mAO[i][j][3] };
                float sky = mSky[i][j] * (1.0f / 15.0f);
                float blk = mBlk[i][j] * (1.0f / 14.0f);
                if (!mUniform[i][j]) {                    // AO gradient → keep 1x1
                    mUsed[i][j] = 1;
                    emitQuad(fi, nAxis, u, v, s, i, j, 1, 1, mTile[i][j], sky, blk, ao);
                    continue;
                }

                uint16_t tile = mTile[i][j];
                uint8_t  sk   = mSky[i][j];
                uint8_t  bk   = mBlk[i][j];
                int L = ao[0];
                auto mergeable = [&](int ii, int jj) {
                    return mValid[ii][jj] && !mUsed[ii][jj] && mUniform[ii][jj]
                        && mTile[ii][jj] == tile && mSky[ii][jj] == sk
                        && mBlk[ii][jj] == bk && mAO[ii][jj][0] == L;
                };
                int w = 1;
                while (i + w < dimU && mergeable(i + w, j)) w++;
                int h = 1;
                for (bool grow = true; grow && j + h < dimV; ) {
                    for (int k = 0; k < w; k++) if (!mergeable(i + k, j + h)) { grow = false; break; }
                    if (grow) h++;
                }
                for (int dj = 0; dj < h; dj++)
                for (int di = 0; di < w; di++) mUsed[i+di][j+dj] = 1;

                int ao4[4] = { L, L, L, L };
                emitQuad(fi, nAxis, u, v, s, i, j, w, h, tile, sky, blk, ao4);
            }
        }
    }

    idxCount  = (int)indices.size();
    vertCount = (int)(verts.size() / 11);
    if (idxCount == 0) { dirty = false; return; }

    if (!vao) {
        pglGenVertexArrays(1, &vao);
        pglGenBuffers(1, &vbo);
        pglGenBuffers(1, &ibo);
    }

    pglBindVertexArray(vao);

    pglBindBuffer(GL_ARRAY_BUFFER, vbo);
    pglBufferData(GL_ARRAY_BUFFER,
                  (GLsizeiptr)(verts.size() * sizeof(float)),
                  verts.data(), GL_DYNAMIC_DRAW);

    const GLsizei stride = 11 * (GLsizei)sizeof(float);
    if (attr.pos >= 0) {
        pglEnableVertexAttribArray((GLuint)attr.pos);
        pglVertexAttribPointer((GLuint)attr.pos, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    }
    if (attr.uv >= 0) {
        pglEnableVertexAttribArray((GLuint)attr.uv);
        pglVertexAttribPointer((GLuint)attr.uv, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
    }
    if (attr.tile >= 0) {
        pglEnableVertexAttribArray((GLuint)attr.tile);
        pglVertexAttribPointer((GLuint)attr.tile, 2, GL_FLOAT, GL_FALSE, stride, (void*)(5*sizeof(float)));
    }
    if (attr.light >= 0) {
        pglEnableVertexAttribArray((GLuint)attr.light);
        pglVertexAttribPointer((GLuint)attr.light, 1, GL_FLOAT, GL_FALSE, stride, (void*)(7*sizeof(float)));
    }
    if (attr.sky >= 0) {
        pglEnableVertexAttribArray((GLuint)attr.sky);
        pglVertexAttribPointer((GLuint)attr.sky, 1, GL_FLOAT, GL_FALSE, stride, (void*)(8*sizeof(float)));
    }
    if (attr.block >= 0) {
        pglEnableVertexAttribArray((GLuint)attr.block);
        pglVertexAttribPointer((GLuint)attr.block, 1, GL_FLOAT, GL_FALSE, stride, (void*)(9*sizeof(float)));
    }
    if (attr.wave >= 0) {
        pglEnableVertexAttribArray((GLuint)attr.wave);
        pglVertexAttribPointer((GLuint)attr.wave, 1, GL_FLOAT, GL_FALSE, stride, (void*)(10*sizeof(float)));
    }

    pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    pglBufferData(GL_ELEMENT_ARRAY_BUFFER,
                  (GLsizeiptr)(indices.size() * sizeof(unsigned int)),
                  indices.data(), GL_DYNAMIC_DRAW);

    pglBindVertexArray(0);
    dirty = false;
}

void Chunk::render() const {
    if (!vao || idxCount == 0) return;
    pglBindVertexArray(vao);
    pglDrawElements(GL_TRIANGLES, idxCount, GL_UNSIGNED_INT, (void*)0);
    pglBindVertexArray(0);
}

void Chunk::cleanup() {
    if (ibo) { pglDeleteBuffers(1, &ibo);       ibo = 0; }
    if (vbo) { pglDeleteBuffers(1, &vbo);       vbo = 0; }
    if (vao) { pglDeleteVertexArrays(1, &vao);  vao = 0; }
    idxCount = 0;
    dirty = true;
}
