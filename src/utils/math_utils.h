
#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <cmath>

// Matrix math utilities — header-only, zero dependencies.
// All matrices are float[16] in column-major order (OpenGL convention).
// Column-major: m[col*4 + row], so m[0..3] = column 0, m[4..7] = column 1, etc.

static inline void mat4_identity(float* m) {
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

// Perspective projection matrix.
// fovY: field of view in radians (vertical).
// aspect: width / height.
// nearZ, farZ: clipping planes (must be > 0).
static inline void mat4_perspective(float* m, float fovY, float aspect,
                                    float nearZ, float farZ) {
    float f = 1.0f / tanf(fovY * 0.5f);
    float rangeInv = 1.0f / (nearZ - farZ);

    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0]  = f / aspect;
    m[5]  = f;
    m[10] = (farZ + nearZ) * rangeInv;
    m[11] = -1.0f;
    m[14] = 2.0f * farZ * nearZ * rangeInv;
}

// Look-at view matrix.
// eye: camera position.
// center: target point.
// up: up vector (typically 0,1,0).
static inline void mat4_lookAt(float* m,
                                float eyeX, float eyeY, float eyeZ,
                                float centerX, float centerY, float centerZ,
                                float upX, float upY, float upZ) {
    // Forward = normalize(center - eye)
    float fx = centerX - eyeX;
    float fy = centerY - eyeY;
    float fz = centerZ - eyeZ;
    float fLen = sqrtf(fx*fx + fy*fy + fz*fz);
    fx /= fLen; fy /= fLen; fz /= fLen;

    // Side = normalize(forward x up)
    float sx = fy * upZ - fz * upY;
    float sy = fz * upX - fx * upZ;
    float sz = fx * upY - fy * upX;
    float sLen = sqrtf(sx*sx + sy*sy + sz*sz);
    sx /= sLen; sy /= sLen; sz /= sLen;

    // Up = side x forward
    float ux = sy * fz - sz * fy;
    float uy = sz * fx - sx * fz;
    float uz = sx * fy - sy * fx;

    // Column-major layout
    m[0]  = sx;   m[1]  = ux;   m[2]  = -fx;  m[3]  = 0.0f;
    m[4]  = sy;   m[5]  = uy;   m[6]  = -fy;  m[7]  = 0.0f;
    m[8]  = sz;   m[9]  = uz;   m[10] = -fz;  m[11] = 0.0f;
    m[12] = -(sx*eyeX + sy*eyeY + sz*eyeZ);
    m[13] = -(ux*eyeX + uy*eyeY + uz*eyeZ);
    m[14] =  (fx*eyeX + fy*eyeY + fz*eyeZ);
    m[15] = 1.0f;
}

// Multiply two 4x4 matrices: result = a * b.
static inline void mat4_multiply(float* result, const float* a, const float* b) {
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a[k * 4 + row] * b[col * 4 + k];
            }
            result[col * 4 + row] = sum;
        }
    }
}

#endif // MATH_UTILS_H
