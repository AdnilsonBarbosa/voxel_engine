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

        GLuint vs = compile(GL_VERTEX_SHADER, VS);
        GLuint fs = compile(GL_FRAGMENT_SHADER, FS);
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

    void draw(float bx, float by, float bz, const float* mvp) {
        if (!program_) return;

        // Slight expansion to avoid z-fighting with block faces
        float e = 0.005f;
        float ox = bx - e, oy = by - e, oz = bz - e;
        float sx = bx + 1.0f + e, sy = by + 1.0f + e, sz = bz + 1.0f + e;

        // Update vertex positions for this block
        float verts[24 * 3];
        for (int i = 0; i < 24; i++) {
            verts[i*3 + 0] = ox + cubeVerts_[i*3 + 0] * (sx - ox);
            verts[i*3 + 1] = oy + cubeVerts_[i*3 + 1] * (sy - oy);
            verts[i*3 + 2] = oz + cubeVerts_[i*3 + 2] * (sz - oz);
        }

        pglUseProgram(program_);
        pglUniformMatrix4fv(locMVP_, 1, GL_FALSE, mvp);
        // Black outline with slight transparency
        pglUniform4f(locColor_, 0.0f, 0.0f, 0.0f, 0.85f);

        pglBindVertexArray(vao_);
        pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
        pglBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        pglLineWidth(2.0f);
        pglDrawArrays(GL_LINES, 0, 24);
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
