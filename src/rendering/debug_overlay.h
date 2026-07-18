#pragma once
// DebugOverlay — minimal 2D bitmap-font renderer for on-screen debug info.
// Uses a built-in 8x8 ASCII font, no external files or SDL_ttf needed.
// Renders as textured quads via a dedicated GL program.
#include "gl_ext.h"

class DebugOverlay {
public:
    void init();
    void cleanup();

    // Begin a new frame (call before any drawText).
    void beginFrame(int screenW, int screenH);

    // Draw text at pixel position (x,y from top-left). Color is 0xRRGGBB.
    void drawText(const char* text, int x, int y, unsigned int color = 0xFFFFFF, float scale = 1.0f);

    // Draw a filled rectangle (for button backgrounds, etc.)
    void drawRect(int x, int y, int w, int h, unsigned int color, float alpha = 0.8f);

    // ── Textured icons (block previews from the texture atlas) ──────────────
    // Set once per frame; drawIcon quads sample this RGBA texture directly.
    void setIconTexture(GLuint tex) { iconTex_ = tex; }
    void drawIcon(int x, int y, int w, int h,
                  float u0, float v0, float u1, float v1,
                  unsigned int tint = 0xFFFFFFu, float alpha = 1.0f);

    void drawIconQuad(float x0, float y0, float x1, float y1,
                      float x2, float y2, float x3, float y3,
                      float u0, float v0, float u1, float v1,
                      unsigned int tint = 0xFFFFFFu, float alpha = 1.0f);
    // Flush all draw calls to the GPU (call after all drawText/drawRect).
    void flush();

private:
    GLuint program = 0;
    GLuint fontTex = 0;
    GLuint vao = 0, vbo = 0;
    GLint uProj = -1, uTex = -1, uRGBA = -1;
    GLuint iconTex_ = 0;

    // Batched quads: each quad = 6 vertices (2 triangles), each vertex = pos(2) + uv(2) + color(4) = 8 floats
    static const int MAX_QUADS = 2048;
    float quadData[MAX_QUADS * 6 * 8];
    int quadCount = 0;
    int screenW_ = 0, screenH_ = 0;

    // Draw-order-preserving texture segments: consecutive quads that share a
    // texture draw as one call; switching font↔atlas starts a new segment.
    struct Seg { GLuint tex; int rgba; int quads; };
    static const int MAX_SEGS = 256;
    Seg segs[MAX_SEGS];
    int segCount = 0;

    void pushQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  unsigned int color, float alpha);
    void pushQuadSeg(GLuint tex, int rgba,
                     float x, float y, float w, float h,
                     float u0, float v0, float u1, float v1,
                     unsigned int color, float alpha);
    void pushQuadVertices(GLuint tex, int rgba,
                          const float* xy, const float* uv,
                          unsigned int color, float alpha);
    void flushBatch();
};
