#pragma once
#include "gl_ext.h"

// Atmospheric sky: gradient + sunrise/sunset + stars + animated sun/moon +
// wind-driven clouds. Rendered as a single fullscreen triangle.
class Sky {
public:
    void init();
    // cloudCover  = 0..1   (weather cloud density)
    // lightMult   = 0..1   (storm dimmer — storms darken sky)
    // windX/Z     = wind world-space XZ components (drives cloud drift direction)
    // cloudRGB    = cloud colour interpolated by WeatherManager
    // fogDensity  = 0..1   (atmospheric horizon haze)
    // sunElevation= sun.y component (-1..+1) — used for day/night detection
    void render(const float camRight[3], const float camUp[3], const float camFwd[3],
                float tanX, float tanY, const float sunDir[3], float time,
                const float ambient[3],
                float cloudCover    = 0.1f,
                float lightMult     = 1.0f,
                float windX         = 0.0f,
                float windZ         = 0.0f,
                const float cloudRGB[3] = nullptr,
                float fogDensity    = 0.0f);
    void cleanup();

private:
    GLuint program = 0, vao = 0, vbo = 0;
    GLint  aPos = -1;
    GLint  uCamRight = -1, uCamUp = -1, uCamFwd = -1;
    GLint  uTanX = -1, uTanY = -1, uTime = -1, uSunDir = -1, uAmbient = -1;
    GLint  uCloudCover = -1, uLightMult = -1;
    GLint  uWindX = -1, uWindZ = -1;
    GLint  uCloudColor = -1;
    GLint  uFogDensity = -1;
};
