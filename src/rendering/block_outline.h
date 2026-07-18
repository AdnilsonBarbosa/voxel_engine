#pragma once
// block_outline.h — Wireframe cube outline for the selected block.
// Header-only, uses a dedicated minimal shader + GL_LINES.
// Draws a slightly expanded cube (1.005x) to avoid z-fighting.
#include "gl_ext.h"
#include "../world/chunk_manager.h"
#include <cmath>

class BlockOutline {
public:
    void init() {
        // Minimal vertex shader: transform 3D position by MVP
        static const char* VS = R"(
#version 130
in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

        // Minimal fragment shader: solid color
        static const char* FS = R"(
#version 130
uniform vec4 uColor;
out vec4 fragColor;
void main() {
    fragColor = uColor;
}
)";

#ifdef __ANDROID__
        static const char* VS_GLES = R"(
#version 100
precision highp float;
attribute vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)";
        static const char* FS_GLES = R"(
#version 100
precision mediump float;
uniform vec4 uColor;
void main() { gl_FragColor = uColor; }
)";
#endif
        auto compile = [](GLenum type, const char* src) -> GLuint {
            GLuint s = pglCreateShader(type);
            pglShaderSource(s, 1, &src, nullptr);
            pglCompileShader(s);
            GLint ok = 0;
            pglGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                char log[512];
                pglGetShaderInfoLog(s, sizeof(log), nullptr, log);
                SDL_Log("[BlockOutline] shader error: %s", log);
                pglDeleteShader(s);
                return 0;
            }
            return s;
        };

#ifdef __ANDROID__
        GLuint vs = compile(GL_VERTEX_SHADER, VS_GLES);
        GLuint fs = compile(GL_FRAGMENT_SHADER, FS_GLES);
#else
        GLuint vs = compile(GL_VERTEX_SHADER, VS);
        GLuint fs = compile(GL_FRAGMENT_SHADER, FS);
#endif
        if (!vs || !fs) return;

        program_ = pglCreateProgram();
        pglAttachShader(program_, vs);
        pglAttachShader(program_, fs);
        pglLinkProgram(program_);
        pglDeleteShader(vs);
        pglDeleteShader(fs);

        GLint ok = 0;
        pglGetProgramiv(program_, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512];
            pglGetProgramInfoLog(program_, sizeof(log), nullptr, log);
            SDL_Log("[BlockOutline] link error: %s", log);
            pglDeleteProgram(program_);
            program_ = 0;
            return;
        }

        locPos_  = pglGetAttribLocation(program_, "aPos");
        locMVP_  = pglGetUniformLocation(program_, "uMVP");
        locColor_= pglGetUniformLocation(program_, "uColor");

        // Build wireframe cube: 12 edges = 24 vertices (GL_LINES)
        buildCube_();

        pglGenVertexArrays(1, &vao_);
        pglGenBuffers(1, &vbo_);
        pglBindVertexArray(vao_);
        pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
        pglBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts_), cubeVerts_, GL_STATIC_DRAW);

        pglEnableVertexAttribArray((GLuint)locPos_);
        pglVertexAttribPointer((GLuint)locPos_, 3, GL_FLOAT, GL_FALSE,
                               3 * sizeof(float), (void*)0);
        pglBindVertexArray(0);
    }

    void draw(float bx, float by, float bz, const float* mvp, bool placement = false, bool valid = true) {
        if (!program_) return;

        pglUseProgram(program_);
        pglUniformMatrix4fv(locMVP_, 1, GL_FALSE, mvp);
        pglBindVertexArray(vao_);
        pglEnable(GL_BLEND);
        pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        auto drawPass = [&](float e, float r, float g, float b, float a, float width) {
            float ox = bx - e, oy = by - e, oz = bz - e;
            float sx = bx + 1.0f + e, sy = by + 1.0f + e, sz = bz + 1.0f + e;
            float verts[24 * 3];
            for (int i = 0; i < 24; i++) {
                verts[i*3 + 0] = ox + cubeVerts_[i*3 + 0] * (sx - ox);
                verts[i*3 + 1] = oy + cubeVerts_[i*3 + 1] * (sy - oy);
                verts[i*3 + 2] = oz + cubeVerts_[i*3 + 2] * (sz - oz);
            }
            pglUniform4f(locColor_, r, g, b, a);
            pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
            pglBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
            pglLineWidth(width);
            pglDrawArrays(GL_LINES, 0, 24);
        };

        // Black outer edge identifies the exact block under the crosshair.
        drawPass(0.020f, 0.015f, 0.018f, 0.022f, 0.96f, 4.0f);
        if (placement) {
            drawPass(0.008f, valid ? 0.35f : 0.86f,
                     valid ? 0.82f : 0.18f, valid ? 1.0f : 0.16f,
                     0.92f, 1.5f);
        } else {
            drawPass(0.008f, 1.0f, 0.88f, 0.35f, 0.95f, 1.5f);
        }
        pglDisable(GL_BLEND);
        pglBindVertexArray(0);
    }
    void drawCracks(float bx, float by, float bz, const float* mvp, float progress) {
        if (!program_ || progress <= 0.0f) return;
        static const float crack[12][4] = {
            {0.12f,0.82f,0.43f,0.56f}, {0.43f,0.56f,0.25f,0.24f},
            {0.43f,0.56f,0.78f,0.70f}, {0.78f,0.70f,0.92f,0.38f},
            {0.43f,0.56f,0.62f,0.90f}, {0.62f,0.90f,0.76f,0.98f},
            {0.25f,0.24f,0.08f,0.12f}, {0.25f,0.24f,0.56f,0.08f},
            {0.78f,0.70f,0.94f,0.88f}, {0.58f,0.36f,0.82f,0.20f},
            {0.58f,0.36f,0.38f,0.08f}, {0.58f,0.36f,0.66f,0.54f}
        };
        const int count = std::max(1, std::min(12, (int)std::ceil(progress * 12.0f)));
        float verts[12 * 6 * 3] = {};
        int n = 0;
        const float e = 0.014f;
        auto add = [&](float x1, float y1, float z1, float x2, float y2, float z2) {
            if (n + 6 > (int)(sizeof(verts) / sizeof(float))) return;
            verts[n++] = x1; verts[n++] = y1; verts[n++] = z1;
            verts[n++] = x2; verts[n++] = y2; verts[n++] = z2;
        };
        for (int face = 0; face < 6; ++face) {
            for (int i = 0; i < count; ++i) {
                const float x1 = crack[i][0], y1 = crack[i][1];
                const float x2 = crack[i][2], y2 = crack[i][3];
                switch (face) {
                    case 0: add(bx+x1, by+y1, bz-e, bx+x2, by+y2, bz-e); break;
                    case 1: add(bx+x1, by+y1, bz+1.0f+e, bx+x2, by+y2, bz+1.0f+e); break;
                    case 2: add(bx-e, by+y1, bz+x1, bx-e, by+y2, bz+x2); break;
                    case 3: add(bx+1.0f+e, by+y1, bz+x1, bx+1.0f+e, by+y2, bz+x2); break;
                    case 4: add(bx+x1, by-e, bz+y1, bx+x2, by-e, bz+y2); break;
                    default: add(bx+x1, by+1.0f+e, bz+y1, bx+x2, by+1.0f+e, bz+y2); break;
                }
            }
        }
        pglUseProgram(program_);
        pglUniformMatrix4fv(locMVP_, 1, GL_FALSE, mvp);
        pglUniform4f(locColor_, 0.015f, 0.012f, 0.010f, 0.92f);
        pglBindVertexArray(vao_);
        pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
        // Keep the VBO large enough for the following outline pass too.
        pglBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
        pglEnable(GL_BLEND);
        pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        pglLineWidth(2.5f);
        pglDrawArrays(GL_LINES, 0, n / 3);
        pglDisable(GL_BLEND);
        pglBindVertexArray(0);
    }
    void cleanup() {
        if (vbo_)  { pglDeleteBuffers(1, &vbo_);  vbo_ = 0; }
        if (vao_)  { pglDeleteVertexArrays(1, &vao_); vao_ = 0; }
        if (program_) { pglDeleteProgram(program_); program_ = 0; }
    }

private:
    GLuint program_ = 0;
    GLuint vao_ = 0, vbo_ = 0;
    GLint  locPos_ = -1, locMVP_ = -1, locColor_ = -1;

    // Unit cube vertices for 12 edges (24 vertices, GL_LINES)
    // Each vertex is (x,y,z) in [0,1] range, will be scaled to block bounds
    float cubeVerts_[24 * 3];

    void buildCube_() {
        // 8 corners of a unit cube
        float c[8][3] = {
            {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
            {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
        };
        // 12 edges: pairs of corner indices
        int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},  // bottom face
            {4,5},{5,6},{6,7},{7,4},  // top face
            {0,4},{1,5},{2,6},{3,7}   // vertical edges
        };
        for (int i = 0; i < 12; i++) {
            int a = edges[i][0], b = edges[i][1];
            cubeVerts_[i*6 + 0] = c[a][0];
            cubeVerts_[i*6 + 1] = c[a][1];
            cubeVerts_[i*6 + 2] = c[a][2];
            cubeVerts_[i*6 + 3] = c[b][0];
            cubeVerts_[i*6 + 4] = c[b][1];
            cubeVerts_[i*6 + 5] = c[b][2];
        }
    }
};
