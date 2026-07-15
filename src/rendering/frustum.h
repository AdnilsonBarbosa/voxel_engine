#pragma once
#include <cmath>

// View frustum extracted from a column-major MVP matrix (Gribb-Hartmann).
// Used to skip drawing chunks that are completely off-screen.
struct Frustum {
    float pl[6][4];   // 6 planes: a,b,c,d  (ax+by+cz+d >= 0 means inside)

    // m is column-major float[16] as produced by mat4_multiply().
    // Row r of the matrix = ( m[r], m[4+r], m[8+r], m[12+r] ).
    void fromMatrix(const float* m) {
        // rows
        float r0[4] = { m[0], m[4], m[8],  m[12] };
        float r1[4] = { m[1], m[5], m[9],  m[13] };
        float r2[4] = { m[2], m[6], m[10], m[14] };
        float r3[4] = { m[3], m[7], m[11], m[15] };

        for (int i = 0; i < 4; i++) {
            pl[0][i] = r3[i] + r0[i];   // left
            pl[1][i] = r3[i] - r0[i];   // right
            pl[2][i] = r3[i] + r1[i];   // bottom
            pl[3][i] = r3[i] - r1[i];   // top
            pl[4][i] = r3[i] + r2[i];   // near
            pl[5][i] = r3[i] - r2[i];   // far
        }
        for (int p = 0; p < 6; p++) {
            float len = sqrtf(pl[p][0]*pl[p][0] + pl[p][1]*pl[p][1] + pl[p][2]*pl[p][2]);
            if (len > 0.0f) { pl[p][0]/=len; pl[p][1]/=len; pl[p][2]/=len; pl[p][3]/=len; }
        }
    }

    // Axis-aligned box test using the positive-vertex optimization.
    bool aabbVisible(float minx, float miny, float minz,
                     float maxx, float maxy, float maxz) const {
        for (int p = 0; p < 6; p++) {
            float a = pl[p][0], b = pl[p][1], c = pl[p][2], d = pl[p][3];
            float px = (a > 0) ? maxx : minx;
            float py = (b > 0) ? maxy : miny;
            float pz = (c > 0) ? maxz : minz;
            if (a*px + b*py + c*pz + d < 0.0f) return false;   // fully outside
        }
        return true;
    }
};
