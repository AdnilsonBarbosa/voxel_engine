#pragma once
#include "gl_ext.h"
#include "../world/chunk.h"
#include <cmath>

class BuildPreview {
public:
    void init() {
        static const char* VS = R"(
#version 130
in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)";
        static const char* FS = R"(
#version 130
uniform vec4 uColor;
out vec4 fragColor;
void main() { fragColor = uColor; }
)";
        auto compile = [](GLenum type, const char* src) -> GLuint {
            GLuint s = pglCreateShader(type);
            pglShaderSource(s, 1, &src, nullptr);
            pglCompileShader(s);
            GLint ok = 0; pglGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok) { char log[512]; pglGetShaderInfoLog(s, sizeof(log), nullptr, log); SDL_Log("[BuildPreview] shader error: %s", log); pglDeleteShader(s); return 0; }
            return s;
        };
        GLuint vs = compile(GL_VERTEX_SHADER, VS), fs = compile(GL_FRAGMENT_SHADER, FS);
        if (!vs || !fs) return;
        program_ = pglCreateProgram();
        pglAttachShader(program_, vs); pglAttachShader(program_, fs); pglLinkProgram(program_);
        pglDeleteShader(vs); pglDeleteShader(fs);
        GLint ok = 0; pglGetProgramiv(program_, GL_LINK_STATUS, &ok);
        if (!ok) { char log[512]; pglGetProgramInfoLog(program_, sizeof(log), nullptr, log); SDL_Log("[BuildPreview] link error: %s", log); pglDeleteProgram(program_); program_ = 0; return; }
        locPos_ = pglGetAttribLocation(program_, "aPos");
        locMVP_ = pglGetUniformLocation(program_, "uMVP");
        locColor_ = pglGetUniformLocation(program_, "uColor");
        buildUnit_();
        pglGenVertexArrays(1, &vao_);
        pglGenBuffers(1, &vbo_);
        pglBindVertexArray(vao_);
        pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
        pglBufferData(GL_ARRAY_BUFFER, sizeof(verts_), verts_, GL_DYNAMIC_DRAW);
        pglEnableVertexAttribArray((GLuint)locPos_);
        pglVertexAttribPointer((GLuint)locPos_, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        pglBindVertexArray(0);
    }

    void draw(float bx, float by, float bz, bool valid, const float* mvp) {
        if (!program_) return;
        if (by < 0.0f || by >= (float)CHUNK_H) return;
        float out[36 * 3];
        const float e = 0.01f;
        for (int i = 0; i < 36; ++i) {
            out[i*3+0] = bx - e + verts_[i*3+0] * (1.0f + e * 2.0f);
            out[i*3+1] = by - e + verts_[i*3+1] * (1.0f + e * 2.0f);
            out[i*3+2] = bz - e + verts_[i*3+2] * (1.0f + e * 2.0f);
        }
        pglUseProgram(program_);
        pglUniformMatrix4fv(locMVP_, 1, GL_FALSE, mvp);
        if (valid) pglUniform4f(locColor_, 0.15f, 1.0f, 0.40f, 0.28f);
        else       pglUniform4f(locColor_, 1.0f, 0.18f, 0.16f, 0.30f);
        pglBindVertexArray(vao_);
        pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
        pglBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(out), out);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        pglDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        pglBindVertexArray(0);
    }

    void cleanup() {
        if (vbo_) { pglDeleteBuffers(1, &vbo_); vbo_ = 0; }
        if (vao_) { pglDeleteVertexArrays(1, &vao_); vao_ = 0; }
        if (program_) { pglDeleteProgram(program_); program_ = 0; }
    }

private:
    GLuint program_ = 0, vao_ = 0, vbo_ = 0;
    GLint locPos_ = -1, locMVP_ = -1, locColor_ = -1;
    float verts_[36 * 3] = {};

    void addTri_(int& i, float ax,float ay,float az, float bx,float by,float bz, float cx,float cy,float cz) {
        float v[9] = {ax,ay,az,bx,by,bz,cx,cy,cz};
        for (int n = 0; n < 9; ++n) verts_[i++] = v[n];
    }
    void buildUnit_() {
        int i = 0;
        addTri_(i,0,0,0, 1,0,0, 1,1,0); addTri_(i,0,0,0, 1,1,0, 0,1,0);
        addTri_(i,1,0,1, 0,0,1, 0,1,1); addTri_(i,1,0,1, 0,1,1, 1,1,1);
        addTri_(i,0,0,1, 0,0,0, 0,1,0); addTri_(i,0,0,1, 0,1,0, 0,1,1);
        addTri_(i,1,0,0, 1,0,1, 1,1,1); addTri_(i,1,0,0, 1,1,1, 1,1,0);
        addTri_(i,0,1,0, 1,1,0, 1,1,1); addTri_(i,0,1,0, 1,1,1, 0,1,1);
        addTri_(i,0,0,1, 1,0,1, 1,0,0); addTri_(i,0,0,1, 1,0,0, 0,0,0);
    }
};
