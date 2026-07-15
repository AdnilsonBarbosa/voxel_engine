#pragma once
// collision.h — Player physics and voxel raycast.
// Header-only, zero heap allocation, mobile-friendly.
//
// Approach: move player, then resolve ALL overlaps by pushing out along
// the axis of least penetration. Repeat until clear.
#include "../world/chunk_manager.h"
#include "../world/block_types.h"
#include <cmath>
#include "SDL.h"

// ── Player constants ───────────────────────────────────────────────────────
static constexpr float PLAYER_WIDTH  = 0.6f;
static constexpr float PLAYER_HEIGHT = 1.74f;
static constexpr float EYE_HEIGHT    = 1.62f;
static constexpr float GRAVITY       = 25.0f;
static constexpr float JUMP_VELOCITY = 8.5f;
static constexpr float WALK_SPEED    = 4.317f;
static constexpr float FRICTION_GROUND = 14.0f;
static constexpr float FRICTION_AIR    = 3.0f;
static constexpr float MAX_REACH       = 5.0f;
static constexpr float TERMINAL_VEL    = -50.0f;

// ── Helpers ─────────────────────────────────────────────────────────────────
static inline float fastAbs(float v) { return v < 0 ? -v : v; }
static inline int fastFloor(float v) { return (int)v - ((int)v > v ? 1 : 0); }
static inline int fastSign(float v)  { return (v > 0) ? 1 : ((v < 0) ? -1 : 0); }

// ── Check if a block at integer coords is solid ────────────────────────────
static inline bool isSolidAt(int bx, int by, int bz, const ChunkManager& world) {
    if (by < 0 || by >= 128) return false;
    uint8_t blk = world.getBlock((float)bx + 0.5f, (float)by + 0.5f, (float)bz + 0.5f);
    return blockIsOpaque(blk);
}

// ── Ground check: 5 points below feet ──────────────────────────────────────
static inline bool isOnGround(float px, float py, float pz, const ChunkManager& world) {
    float hw = PLAYER_WIDTH * 0.5f - 0.02f;
    float ty = py - 0.05f;
    float offsets[5][2] = {
        { -hw, -hw }, { hw, -hw }, { -hw, hw }, { hw, hw }, { 0, 0 }
    };
    for (int i = 0; i < 5; i++) {
        int bx = fastFloor(px + offsets[i][0]);
        int by = fastFloor(ty);
        int bz = fastFloor(pz + offsets[i][1]);
        if (isSolidAt(bx, by, bz, world)) return true;
    }
    return false;
}

// ── Player physics state ───────────────────────────────────────────────────
struct PhysicsPlayer {
    float x, y, z;
    float vx, vy, vz;
    bool  grounded;
};

// ── Resolve ALL overlaps: push player out of any solid block ────────────────
// For each overlapping block, compute the shortest push-out direction.
// Apply the smallest one. Repeat until clear.
static inline void resolveAllOverlaps(PhysicsPlayer& p, const ChunkManager& world) {
    float hw = PLAYER_WIDTH * 0.5f;

    for (int iter = 0; iter < 10; iter++) {
        float minX = p.x - hw, maxX = p.x + hw;
        float minY = p.y,      maxY = p.y + PLAYER_HEIGHT;
        float minZ = p.z - hw, maxZ = p.z + hw;

        int bx0 = fastFloor(minX), bx1 = fastFloor(maxX - 0.001f);
        int by0 = fastFloor(minY), by1 = fastFloor(maxY - 0.001f);
        int bz0 = fastFloor(minZ), bz1 = fastFloor(maxZ - 0.001f);

        float bestDist = 1e30f;
        float pushX = 0, pushY = 0, pushZ = 0;
        bool found = false;

        for (int bx = bx0; bx <= bx1; bx++)
        for (int by = by0; by <= by1; by++)
        for (int bz = bz0; bz <= bz1; bz++) {
            if (!isSolidAt(bx, by, bz, world)) continue;
            // Block AABB: [bx, bx+1] x [by, by+1] x [bz, bz+1]
            // Check overlap
            if (maxX <= (float)bx || minX >= (float)(bx+1)) continue;
            if (maxZ <= (float)bz || minZ >= (float)(bz+1)) continue;
            if (maxY <= (float)by || minY >= (float)(by+1)) continue;

            // Compute push-out distances for each axis
            // Push left:  player right (maxX) to block left (bx)
            float pushLeft  = (float)bx - maxX;
            // Push right: player left (minX) to block right (bx+1)
            float pushRight = (float)(bx + 1) - minX;
            // Push down: player top (maxY) to block bottom (by)
            float pushDown  = (float)by - maxY;
            // Push up:   player bottom (minY) to block top (by+1)
            float pushUp    = (float)(by + 1) - minY;
            // Push back: player front (maxZ) to block back (bz)
            float pushBack  = (float)bz - maxZ;
            // Push fwd:  player back (minZ) to block front (bz+1)
            float pushFwd   = (float)(bz + 1) - minZ;

            // Find the smallest positive push
            float pushes[6] = { pushLeft, pushRight, pushDown, pushUp, pushBack, pushFwd };
            for (int a = 0; a < 6; a++) {
                if (pushes[a] > 0.0f && pushes[a] < bestDist) {
                    bestDist = pushes[a];
                    pushX = 0; pushY = 0; pushZ = 0;
                    switch (a) {
                        case 0: pushX = pushLeft;  break; // push left (negative X)
                        case 1: pushX = pushRight; break; // push right (positive X)
                        case 2: pushY = pushDown;  break; // push down (negative Y)
                        case 3: pushY = pushUp;    break; // push up (positive Y)
                        case 4: pushZ = pushBack;  break; // push back (negative Z)
                        case 5: pushZ = pushFwd;   break; // push fwd (positive Z)
                    }
                    found = true;
                }
            }
        }

        if (!found) break;  // no more overlaps
        p.x += pushX;
        p.y += pushY;
        p.z += pushZ;

        // If pushed up, we landed
        if (pushY > 0 && p.vy <= 0) {
            p.vy = 0;
            p.grounded = true;
        }
        // If pushed down, hit ceiling
        if (pushY < 0 && p.vy > 0) {
            p.vy = 0;
        }
    }
}

// ── Resolve player collision ───────────────────────────────────────────────
static inline void resolveCollision(PhysicsPlayer& p, const ChunkManager& world, float dt) {
    // Apply gravity
    p.vy -= GRAVITY * dt;
    if (p.vy < TERMINAL_VEL) p.vy = TERMINAL_VEL;

    p.grounded = false;

    // Move on each axis independently
    p.x += p.vx * dt;
    p.z += p.vz * dt;
    p.y += p.vy * dt;

    // Resolve ALL overlaps (pushes out on shortest axis)
    resolveAllOverlaps(p, world);

    // Ground check
    p.grounded = isOnGround(p.x, p.y, p.z, world);

    // Safety
    if (p.y < 0) { p.y = 0; p.vy = 0; p.grounded = true; }
}

// ── Voxel traversal raycast (Amanatides & Woo) ─────────────────────────────
struct RaycastResult {
    int   blockX, blockY, blockZ;
    int   faceX, faceY, faceZ;
    float hitDist;
    float placeX, placeY, placeZ;
    bool  hit = false;
};

static inline bool raycastVoxel(const ChunkManager& world,
                                float ox, float oy, float oz,
                                float dx, float dy, float dz,
                                float maxDist,
                                RaycastResult& result) {
    result.hit = false;
    int x = fastFloor(ox), y = fastFloor(oy), z = fastFloor(oz);
    int stepX = fastSign(dx), stepY = fastSign(dy), stepZ = fastSign(dz);
    float tDeltaX = (dx != 0.0f) ? fastAbs(1.0f / dx) : 1e30f;
    float tDeltaY = (dy != 0.0f) ? fastAbs(1.0f / dy) : 1e30f;
    float tDeltaZ = (dz != 0.0f) ? fastAbs(1.0f / dz) : 1e30f;
    float tMaxX = (dx != 0.0f) ? ((dx > 0) ? ((x + 1.0f - ox) / dx) : ((ox - (float)x) / -dx)) : 1e30f;
    float tMaxY = (dy != 0.0f) ? ((dy > 0) ? ((y + 1.0f - oy) / dy) : ((oy - (float)y) / -dy)) : 1e30f;
    float tMaxZ = (dz != 0.0f) ? ((dz > 0) ? ((z + 1.0f - oz) / dz) : ((oz - (float)z) / -dz)) : 1e30f;
    float t = 0.0f;
    int faceX = 0, faceY = 0, faceZ = 0;
    for (int i = 0; i < 200 && t <= maxDist; i++) {
        uint8_t blk = world.getBlock((float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f);
        if (blk != BLOCK_AIR) {
            result.blockX = x; result.blockY = y; result.blockZ = z;
            result.faceX = faceX; result.faceY = faceY; result.faceZ = faceZ;
            result.hitDist = t;
            result.placeX = (float)(x + faceX); result.placeY = (float)(y + faceY); result.placeZ = (float)(z + faceZ);
            result.hit = true;
            return true;
        }
        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) { x += stepX; t = tMaxX; tMaxX += tDeltaX; faceX = -stepX; faceY = 0; faceZ = 0; }
            else               { z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ; faceX = 0; faceY = 0; faceZ = -stepZ; }
        } else {
            if (tMaxY < tMaxZ) { y += stepY; t = tMaxY; tMaxY += tDeltaY; faceX = 0; faceY = -stepY; faceZ = 0; }
            else               { z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ; faceX = 0; faceY = 0; faceZ = -stepZ; }
        }
    }
    return false;
}

// ── Placement overlap check ────────────────────────────────────────────────
static inline bool placementOverlapsPlayer(float bx, float by, float bz,
                                           float playerX, float playerY, float playerZ) {
    float hw = PLAYER_WIDTH * 0.5f + 0.01f;
    float pMinX = playerX - hw, pMaxX = playerX + hw;
    float pMinY = playerY,     pMaxY = playerY + PLAYER_HEIGHT;
    float pMinZ = playerZ - hw, pMaxZ = playerZ + hw;
    int ibx = fastFloor(bx), iby = fastFloor(by), ibz = fastFloor(bz);
    return (pMaxX > (float)ibx && pMinX < (float)(ibx+1) &&
            pMaxY > (float)iby && pMinY < (float)(iby+1) &&
            pMaxZ > (float)ibz && pMinZ < (float)(ibz+1));
}
