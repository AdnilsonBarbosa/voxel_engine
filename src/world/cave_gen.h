#pragma once
#include "noise.h"
#include <cmath>
#include <cstdint>

// CaveGenerator — 3D-noise carving that removes solid blocks to form natural
// caves. Nothing is allocated: the terrain pass simply asks carve()/ravine()
// whether each underground voxel should become air. Designed to avoid square
// grids and repeating patterns:
//   * Tunnels   — intersection of two 3D fields → winding "spaghetti" channels
//   * Rooms     — low-frequency 3D blobs open up occasional large chambers
//   * Ravines   — a rare 2D ridge zero-crossing cuts thin vertical canyons
namespace Caves {

// True → this underground position should be carved out to air.
inline bool carve(int wx, int y, int wz, unsigned seed) {
    // Slightly squash Y so tunnels run more horizontally than vertically.
    float fy = (float)y * 1.5f;

    // Two independent fields both near zero trace a 1D channel through 3D space.
    float a = Noise::fbm3((float)wx, fy, (float)wz, seed + 21u, 2, 0.026f);
    float b = Noise::fbm3((float)wx, fy, (float)wz, seed + 53u, 2, 0.026f);
    if (a * a + b * b < 0.022f) return true;

    // Large rooms: rarer, deeper low-frequency blobs.
    if (y < 46) {
        float r = Noise::fbm3((float)wx, (float)y, (float)wz, seed + 88u, 3, 0.013f);
        if (r > 0.60f) return true;
    }
    return false;
}

// Whether this column lies on a ravine (constant for the whole column, so the
// terrain pass evaluates it once per column instead of once per voxel).
inline bool ravineColumn(int wx, int wz, unsigned seed) {
    float r = Noise::fbm((float)wx, (float)wz, seed + 404u, 2, 0.006f);
    return fabsf(r) < 0.010f;   // thin band around the ridge zero-crossing
}

// Vertical extent of a ravine.
inline bool ravineY(int y) { return y >= 12 && y <= 72; }

} // namespace Caves
