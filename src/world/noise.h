#pragma once
#include <cmath>
#include <cstdint>

// Fast deterministic value noise. No external dependencies.
// All functions are pure (same seed + coords = same result).
namespace Noise {

static inline float _s(float t)           { return t*t*(3.f-2.f*t); }
static inline float _l(float a,float b,float t){ return a+t*(b-a); }

static inline float _h(int x, int z, unsigned seed) {
    unsigned n = (unsigned)(x*374761393u ^ (unsigned)z*668265263u ^ seed*2246822519u);
    n ^= n>>13; n *= 1274126177u; n ^= n>>16;
    return (float)(n & 0x7FFFFFFFu) * (1.f/0x7FFFFFFFu) * 2.f - 1.f; // [-1,1]
}

// 2D value noise [-1, 1]
static inline float value2(float x, float z, unsigned seed) {
    int ix=(int)floorf(x), iz=(int)floorf(z);
    float fx=x-ix, fz=z-iz, ux=_s(fx), uz=_s(fz);
    return _l(_l(_h(ix,iz,seed),_h(ix+1,iz,seed),ux),
              _l(_h(ix,iz+1,seed),_h(ix+1,iz+1,seed),ux), uz);
}

// Fractal Brownian Motion — octave sum, result in [-1, 1]
static inline float fbm(float x, float z, unsigned seed,
                         int oct=6, float scale=0.003f,
                         float lacunarity=2.f, float gain=0.5f) {
    float v=0,a=0.5f,f=scale,norm=0;
    for(int i=0;i<oct;i++){
        v+=value2(x*f,z*f,seed+(unsigned)i*137u)*a;
        norm+=a; a*=gain; f*=lacunarity;
    }
    return v/norm;
}

// ── 3D value noise (for caves / ore veins) ──────────────────────────────────
static inline float _h3(int x, int y, int z, unsigned seed) {
    unsigned n = (unsigned)(x*374761393u ^ (unsigned)y*1103515245u ^
                            (unsigned)z*668265263u ^ seed*2246822519u);
    n ^= n>>13; n *= 1274126177u; n ^= n>>16;
    return (float)(n & 0x7FFFFFFFu) * (1.f/0x7FFFFFFFu) * 2.f - 1.f; // [-1,1]
}

// 3D value noise [-1, 1] — trilinear interpolation of 8 lattice corners
static inline float value3(float x, float y, float z, unsigned seed) {
    int ix=(int)floorf(x), iy=(int)floorf(y), iz=(int)floorf(z);
    float fx=x-ix, fy=y-iy, fz=z-iz;
    float ux=_s(fx), uy=_s(fy), uz=_s(fz);
    float x00=_l(_h3(ix,iy,iz,seed),   _h3(ix+1,iy,iz,seed),   ux);
    float x10=_l(_h3(ix,iy+1,iz,seed), _h3(ix+1,iy+1,iz,seed), ux);
    float x01=_l(_h3(ix,iy,iz+1,seed), _h3(ix+1,iy,iz+1,seed), ux);
    float x11=_l(_h3(ix,iy+1,iz+1,seed),_h3(ix+1,iy+1,iz+1,seed),ux);
    return _l(_l(x00,x10,uy), _l(x01,x11,uy), uz);
}

// 3D fractal noise [-1, 1]
static inline float fbm3(float x, float y, float z, unsigned seed,
                         int oct=4, float scale=0.03f,
                         float lacunarity=2.f, float gain=0.5f) {
    float v=0,a=0.5f,f=scale,norm=0;
    for(int i=0;i<oct;i++){
        v+=value3(x*f,y*f,z*f,seed+(unsigned)i*191u)*a;
        norm+=a; a*=gain; f*=lacunarity;
    }
    return v/norm;
}

} // namespace Noise
