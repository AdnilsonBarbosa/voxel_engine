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

    // Flush all draw calls to the GPU (call after all drawText/drawRect).
    void flush();

private:
    GLuint program = 0;
    GLuint fontTex = 0;
    GLuint vao = 0, vbo = 0;
    GLint uProj = -1, uTex = -1;

    // Batched quads: each quad = 6 vertices (2 triangles), each vertex = pos(2) + uv(2) + color(4) = 8 floats
    static const int MAX_QUADS = 2048;
    float quadData[MAX_QUADS * 6 * 8];
    int quadCount = 0;
    int screenW_ = 0, screenH_ = 0;

    void pushQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  unsigned int color, float alpha);
    void flushBatch();
};
