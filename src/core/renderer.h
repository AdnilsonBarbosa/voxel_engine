#pragma once
#include "gl_ext.h"

class Renderer {
public:
    // Textured vertex attribute locations (set after init(), used by ChunkManager).
    GLint locPos   = -1;
    GLint locUV    = -1;
    GLint locTile  = -1;
    GLint locLight = -1;
    GLint locSky   = -1;
    GLint locBlock = -1;
    GLint locWave  = -1;
    // Uniforms
    GLint uMVP      = -1;
    GLint uTime     = -1;
    GLint uCamPos   = -1;
    GLint uAmbient  = -1;
    GLint uFogStart = -1;
    GLint uFogEnd   = -1;
    GLint uAtlas     = -1;
    GLint uTileSize  = -1;
    GLint uTexel     = -1;
    GLint uSunLight  = -1;
    GLint uCaveAmbient = -1;

    void init();
    void beginFrame();                        // clear color+depth

    // Atlas texture + tile metrics (call once after the atlas is built).
    void setAtlas(GLuint tex, float tileSizeUV, float texelUV);

    // Sky/cave lighting for this frame (sunLight follows the day/night cycle).
    void setLight(float sunLight, float caveAmbient) { sunL = sunLight; caveA = caveAmbient; }

    // Activate shader + upload per-frame uniforms (MVP, time, camera, biome fog).
    void setFrame(const float* mvp, float time,
                  const float camPos[3], const float ambient[3],
                  float fogStart, float fogEnd);
    void cleanup();

private:
    GLuint program   = 0;
    GLuint atlasTex  = 0;
    float  tileSize  = 0.0625f;
    float  texel     = 0.00390625f;
    float  sunL      = 1.0f;
    float  caveA     = 0.16f;
};
