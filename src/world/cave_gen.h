#pragma once
#include "noise.h"
#include <cmath>
#include <cstdint>

namespace Caves {

inline bool carve(int wx, int y, int wz, unsigned seed) {
    const float fy = (float)y * 1.5f;
    const float a = Noise::fbm3((float)wx, fy, (float)wz,
                                seed + 21u, 2, 0.026f);
    const float b = Noise::fbm3((float)wx, fy, (float)wz,
                                seed + 53u, 2, 0.026f);
    if (a * a + b * b < 0.020f) return true;

    if (y < 42) {
        const float room = Noise::fbm3((float)wx, (float)y, (float)wz,
                                       seed + 88u, 3, 0.013f);
        if (room > 0.64f) return true;
    }
    return false;
}

inline bool ravineColumn(int wx, int wz, unsigned seed) {
    const float r = Noise::fbm((float)wx, (float)wz,
                               seed + 404u, 2, 0.006f);
    return fabsf(r) < 0.007f;
}

inline bool ravineY(int y) {
    return y >= 16 && y <= 56;
}

} // namespace Caves
