#include "debug_overlay.h"
#include "platform.h"
#include <cstring>

// ── Built-in 8x8 bitmap font (ASCII 32..127) ────────────────────────────────
// Each glyph is 8 bytes; each byte is one row, MSB = leftmost pixel.
// Covers space through tilde — enough for all debug text.
static const uint8_t FONT_8X8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32 (space)
    {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00}, // 33 !
    {0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00}, // 34 "
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, // 35 #
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, // 36 $
    {0x62,0x66,0x0C,0x18,0x30,0x66,0x46,0x00}, // 37 %
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, // 38 &
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, // 39 '
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // 40 (
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // 41 )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // 42 *
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // 43 +
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // 44 ,
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // 45 -
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // 46 .
    {0x02,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // 47 /
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, // 48 0
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 49 1
    {0x3C,0x66,0x06,0x1C,0x30,0x60,0x7E,0x00}, // 50 2
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // 51 3
    {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00}, // 52 4
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // 53 5
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, // 54 6
    {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00}, // 55 7
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // 56 8
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, // 57 9
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // 58 :
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, // 59 ;
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, // 60 <
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // 61 =
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // 62 >
    {0x3C,0x66,0x0C,0x18,0x18,0x00,0x18,0x00}, // 63 ?
    {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0x00}, // 64 @
    {0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x00}, // 65 A
    {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00}, // 66 B
    {0x7C,0xC6,0xC0,0xC0,0xC0,0xC6,0x7C,0x00}, // 67 C
    {0x78,0xCC,0xC6,0xC6,0xC6,0xCC,0x78,0x00}, // 68 D
    {0xFE,0xC0,0xC0,0x7C,0xC0,0xC0,0xFE,0x00}, // 69 E
    {0xFE,0xC0,0xC0,0x7C,0xC0,0xC0,0xC0,0x00}, // 70 F
    {0x7C,0xC6,0xC0,0xC0,0xCE,0xC6,0x7C,0x00}, // 71 G
    {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, // 72 H
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00}, // 73 I
    {0x06,0x06,0x06,0x06,0x06,0xC6,0x7C,0x00}, // 74 J
    {0xCC,0xD8,0xF0,0xE0,0xF0,0xD8,0xCC,0x00}, // 75 K
    {0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xFE,0x00}, // 76 L
    {0xC6,0xEE,0xFE,0xD6,0xC6,0xC6,0xC6,0x00}, // 77 M
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, // 78 N
    {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // 79 O
    {0x7C,0xC6,0xC6,0x7C,0xC0,0xC0,0xC0,0x00}, // 80 P
    {0x7C,0xC6,0xC6,0xC6,0xD6,0xCC,0x76,0x00}, // 81 Q
    {0x7C,0xC6,0xC6,0x7C,0xD8,0xCC,0xC6,0x00}, // 82 R
    {0x7C,0xC6,0xC0,0x7C,0x06,0xC6,0x7C,0x00}, // 83 S
    {0xFE,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // 84 T
    {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, // 85 U
    {0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00}, // 86 V
    {0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00}, // 87 W
    {0xC6,0x6C,0x38,0x38,0x6C,0xC6,0xC6,0x00}, // 88 X
    {0xC6,0xC6,0x6C,0x38,0x18,0x18,0x18,0x00}, // 89 Y
    {0xFE,0x0C,0x18,0x30,0x60,0xC0,0xFE,0x00}, // 90 Z
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // 91 [
    {0x40,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, // 92 backslash
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // 93 ]
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, // 94 ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // 95 _
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // 96 `
    {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, // 97 a
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, // 98 b
    {0x00,0x00,0x3C,0x66,0x60,0x66,0x3C,0x00}, // 99 c
    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, // 100 d
    {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, // 101 e
    {0x1C,0x36,0x30,0x7C,0x30,0x30,0x30,0x00}, // 102 f
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x7C}, // 103 g
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, // 104 h
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // 105 i
    {0x06,0x00,0x06,0x06,0x06,0x66,0x66,0x3C}, // 106 j
    {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, // 107 k
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 108 l
    {0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xC6,0x00}, // 109 m
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, // 110 n
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, // 111 o
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, // 112 p
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, // 113 q
    {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, // 114 r
    {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, // 115 s
    {0x30,0x30,0x7C,0x30,0x30,0x36,0x1C,0x00}, // 116 t
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, // 117 u
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, // 118 v
    {0x00,0x00,0xC6,0xD6,0xD6,0xFE,0x6C,0x00}, // 119 w
    {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, // 120 x
    {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x7C}, // 121 y
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, // 122 z
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // 123 {
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // 124 |
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // 125 }
    {0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00}, // 126 ~
};

// ── Shaders ──────────────────────────────────────────────────────────────────

static const char* OVERLAY_VS = R"(
#version 130
in vec2 aPos;
in vec2 aUV;
in vec4 aColor;
out vec2 vUV;
out vec4 vColor;
uniform mat4 uProj;
void main() {
    vUV    = aUV;
    vColor = aColor;
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
}
)";

static const char* OVERLAY_FS = R"(
#version 130
in vec2 vUV;
in vec4 vColor;
out vec4 fragColor;
uniform sampler2D uTex;
uniform int uRGBA;   // 0 = alpha-only font texture, 1 = full-colour atlas icon
void main() {
    if (uRGBA > 0) {
        vec4 t = texture(uTex, vUV);
        fragColor = vec4(t.rgb * vColor.rgb, t.a * vColor.a);
    } else {
        float a = texture(uTex, vUV).r;
        fragColor = vec4(vColor.rgb, vColor.a * a);
    }
}
)";

#ifdef __ANDROID__
// Android uses the GLES2 context created by SDL. Keep a separate shader pair
// for the 2D overlay; the desktop GLSL pair above is not accepted by GLES2.
static const char* OVERLAY_VS_GLES = R"(
#version 100
precision highp float;
attribute vec2 aPos;
attribute vec2 aUV;
attribute vec4 aColor;
varying vec2 vUV;
varying vec4 vColor;
uniform mat4 uProj;
void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
}
)";

static const char* OVERLAY_FS_GLES = R"(
#version 100
precision mediump float;
varying vec2 vUV;
varying vec4 vColor;
uniform sampler2D uTex;
uniform int uRGBA;
void main() {
    if (uRGBA > 0) {
        vec4 t = texture2D(uTex, vUV);
        gl_FragColor = vec4(t.rgb * vColor.rgb, t.a * vColor.a);
    } else {
        float a = texture2D(uTex, vUV).r;
        gl_FragColor = vec4(vColor.rgb, vColor.a * a);
    }
}
)";
#endif
static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = pglCreateShader(type);
    pglShaderSource(s, 1, &src, nullptr);
    pglCompileShader(s);
    GLint ok = 0;
    pglGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; pglGetShaderInfoLog(s, sizeof(log), nullptr, log);
        SDL_Log("[DebugOverlay] shader error: %s", log);
        pglDeleteShader(s); return 0;
    }
    return s;
}

// ── Font texture generation ──────────────────────────────────────────────────
// Builds a 128x64 grayscale texture atlas from the 8x8 bitmap font.
// Characters 32..127 arranged in 16 columns x 6 rows.

static GLuint buildFontTexture() {
    const int GLYPH_W = 8, GLYPH_H = 8;
    const int COLS = 16, ROWS = 6;
    const int TEX_W = GLYPH_W * COLS;  // 128
    const int TEX_H = GLYPH_H * ROWS;  // 48
    uint8_t pixels[TEX_W * TEX_H];
    memset(pixels, 0, sizeof(pixels));

    for (int ch = 32; ch < 128; ch++) {
        int idx = ch - 32;
        int col = idx % COLS;
        int row = idx / COLS;
        for (int py = 0; py < 8; py++) {
            uint8_t bits = FONT_8X8[idx][py];
            for (int px = 0; px < 8; px++) {
                if (bits & (0x80 >> px)) {
                    int tx = col * GLYPH_W + px;
                    int ty = row * GLYPH_H + py;
                    pixels[ty * TEX_W + tx] = 255;
                }
            }
        }
    }

    GLuint tex = 0;
    pglGenTextures(1, &tex);
    pglBindTexture(GL_TEXTURE_2D, tex);
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, TEX_W, TEX_H, 0,
                  GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

// ── DebugOverlay implementation ──────────────────────────────────────────────

void DebugOverlay::init() {
#ifdef __ANDROID__
    GLuint vs = compileShader(GL_VERTEX_SHADER, OVERLAY_VS_GLES);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, OVERLAY_FS_GLES);
#else
    GLuint vs = compileShader(GL_VERTEX_SHADER, OVERLAY_VS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, OVERLAY_FS);
#endif
    if (!vs || !fs) return;

    program = pglCreateProgram();
    pglAttachShader(program, vs);
    pglAttachShader(program, fs);
    pglLinkProgram(program);
    pglDeleteShader(vs);
    pglDeleteShader(fs);

    GLint ok = 0;
    pglGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; pglGetProgramInfoLog(program, sizeof(log), nullptr, log);
        SDL_Log("[DebugOverlay] link error: %s", log);
        pglDeleteProgram(program); program = 0; return;
    }

    GLint aPos   = pglGetAttribLocation(program, "aPos");
    GLint aUV    = pglGetAttribLocation(program, "aUV");
    GLint aColor = pglGetAttribLocation(program, "aColor");
    uProj = pglGetUniformLocation(program, "uProj");
    uTex  = pglGetUniformLocation(program, "uTex");
    uRGBA = pglGetUniformLocation(program, "uRGBA");

    // VAO + VBO for batched quads: each vertex = pos(2) + uv(2) + color(4) = 8 floats
    pglGenVertexArrays(1, &vao);
    pglGenBuffers(1, &vbo);
    pglBindVertexArray(vao);
    pglBindBuffer(GL_ARRAY_BUFFER, vbo);
    pglBufferData(GL_ARRAY_BUFFER, sizeof(quadData), nullptr, GL_DYNAMIC_DRAW);

    int stride = 8 * sizeof(float);
    pglEnableVertexAttribArray((GLuint)aPos);
    pglVertexAttribPointer((GLuint)aPos,   2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    pglEnableVertexAttribArray((GLuint)aUV);
    pglVertexAttribPointer((GLuint)aUV,    2, GL_FLOAT, GL_FALSE, stride, (void*)(2*sizeof(float)));
    pglEnableVertexAttribArray((GLuint)aColor);
    pglVertexAttribPointer((GLuint)aColor, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4*sizeof(float)));

    pglBindVertexArray(0);

    fontTex = buildFontTexture();
}

void DebugOverlay::cleanup() {
    if (vbo)     { pglDeleteBuffers(1, &vbo);      vbo = 0; }
    if (vao)     { pglDeleteVertexArrays(1, &vao); vao = 0; }
    if (fontTex) { pglDeleteTextures(1, &fontTex); fontTex = 0; }
    if (program) { pglDeleteProgram(program);       program = 0; }
}

void DebugOverlay::beginFrame(int screenW, int screenH) {
    quadCount = 0;
    segCount = 0;
    screenW_ = screenW;
    screenH_ = screenH;
}

void DebugOverlay::pushQuadSeg(GLuint tex, int rgba,
                               float x, float y, float w, float h,
                               float u0, float v0, float u1, float v1,
                               unsigned int color, float alpha) {
    if (quadCount >= MAX_QUADS) return;

    // Extend the current segment or open a new one on texture/mode change.
    if (segCount == 0 || segs[segCount - 1].tex != tex ||
        segs[segCount - 1].rgba != rgba) {
        if (segCount >= MAX_SEGS) return;
        segs[segCount++] = { tex, rgba, 0 };
    }
    segs[segCount - 1].quads++;

    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >>  8) & 0xFF) / 255.0f;
    float b = ((color >>  0) & 0xFF) / 255.0f;

    float* d = &quadData[quadCount * 6 * 8];
    // Triangle 1: TL, TR, BL
    d[ 0] = x;     d[ 1] = y;     d[ 2] = u0; d[ 3] = v0; d[ 4] = r; d[ 5] = g; d[ 6] = b; d[ 7] = alpha;
    d[ 8] = x + w; d[ 9] = y;     d[10] = u1; d[11] = v0; d[12] = r; d[13] = g; d[14] = b; d[15] = alpha;
    d[16] = x;     d[17] = y + h; d[18] = u0; d[19] = v1; d[20] = r; d[21] = g; d[22] = b; d[23] = alpha;
    // Triangle 2: TR, BR, BL
    d[24] = x + w; d[25] = y;     d[26] = u1; d[27] = v0; d[28] = r; d[29] = g; d[30] = b; d[31] = alpha;
    d[32] = x + w; d[33] = y + h; d[34] = u1; d[35] = v1; d[36] = r; d[37] = g; d[38] = b; d[39] = alpha;
    d[40] = x;     d[41] = y + h; d[42] = u0; d[43] = v1; d[44] = r; d[45] = g; d[46] = b; d[47] = alpha;

    quadCount++;
}

void DebugOverlay::pushQuadVertices(GLuint tex, int rgba,
                                    const float* xy, const float* uv,
                                    unsigned int color, float alpha) {
    if (quadCount >= MAX_QUADS) return;
    if (segCount == 0 || segs[segCount - 1].tex != tex ||
        segs[segCount - 1].rgba != rgba) {
        if (segCount >= MAX_SEGS) return;
        segs[segCount++] = { tex, rgba, 0 };
    }
    segs[segCount - 1].quads++;
    const float r = ((color >> 16) & 0xFF) / 255.0f;
    const float g = ((color >> 8) & 0xFF) / 255.0f;
    const float b = (color & 0xFF) / 255.0f;
    float* d = &quadData[quadCount * 6 * 8];
    auto put = [&](int i, int p) {
        d[i * 8 + 0] = xy[p * 2 + 0];
        d[i * 8 + 1] = xy[p * 2 + 1];
        d[i * 8 + 2] = uv[p * 2 + 0];
        d[i * 8 + 3] = uv[p * 2 + 1];
        d[i * 8 + 4] = r; d[i * 8 + 5] = g; d[i * 8 + 6] = b; d[i * 8 + 7] = alpha;
    };
    put(0, 0); put(1, 1); put(2, 2);
    put(3, 0); put(4, 2); put(5, 3);
    ++quadCount;
}
void DebugOverlay::pushQuad(float x, float y, float w, float h,
                            float u0, float v0, float u1, float v1,
                            unsigned int color, float alpha) {
    pushQuadSeg(fontTex, 0, x, y, w, h, u0, v0, u1, v1, color, alpha);
}

void DebugOverlay::drawText(const char* text, int x, int y, unsigned int color, float scale) {
    const int GLYPH_W = 8, GLYPH_H = 8;
    const int COLS = 16;
    const int TEX_W = 128, TEX_H = 48;

    float curX = (float)x;
    for (const char* p = text; *p; p++) {
        int ch = (unsigned char)*p;
        if (ch < 32 || ch > 127) { curX += GLYPH_W * scale; continue; }

        int idx = ch - 32;
        int col = idx % COLS;
        int row = idx / COLS;

        float u0 = (float)(col * GLYPH_W) / TEX_W;
        float v0 = (float)(row * GLYPH_H) / TEX_H;
        float u1 = (float)((col + 1) * GLYPH_W) / TEX_W;
        float v1 = (float)((row + 1) * GLYPH_H) / TEX_H;

        float gw = GLYPH_W * scale;
        float gh = GLYPH_H * scale;
        pushQuad(curX, (float)y, gw, gh, u0, v0, u1, v1, color, 1.0f);
        curX += gw;
    }
}

void DebugOverlay::drawRect(int x, int y, int w, int h, unsigned int color, float alpha) {
    // Solid fill = sample the CENTRE of a guaranteed-solid font texel, so
    // every fragment reads alpha 1. '_' (ASCII 95, col 15 row 3) has a full
    // 0xFF bottom row: texel (120..127, 31). Sampling texel centres avoids
    // the driver-dependent rounding that made rects vanish when a UV landed
    // exactly on a texel boundary.
    const float u = 120.5f / 128.0f;
    const float v = 31.5f / 48.0f;
    pushQuad((float)x, (float)y, (float)w, (float)h, u, v, u, v, color, alpha);
}

void DebugOverlay::drawIcon(int x, int y, int w, int h,
                            float u0, float v0, float u1, float v1,
                            unsigned int tint, float alpha) {
    if (!iconTex_) return;
    pushQuadSeg(iconTex_, 1, (float)x, (float)y, (float)w, (float)h,
                u0, v0, u1, v1, tint, alpha);
}

void DebugOverlay::drawIconQuad(float x0, float y0, float x1, float y1,
                                float x2, float y2, float x3, float y3,
                                float u0, float v0, float u1, float v1,
                                unsigned int tint, float alpha) {
    if (!iconTex_) return;
    const float xy[8] = { x0, y0, x1, y1, x2, y2, x3, y3 };
    const float uv[8] = { u0, v0, u1, v0, u1, v1, u0, v1 };
    pushQuadVertices(iconTex_, 1, xy, uv, tint, alpha);
}
void DebugOverlay::flushBatch() {
    if (quadCount == 0 || !program) return;

    pglUseProgram(program);

    // Orthographic projection: pixel coords (0,0 at top-left)
    float proj[16] = {};
    proj[0]  =  2.0f / (float)screenW_;
    proj[5]  = -2.0f / (float)screenH_;  // flip Y
    proj[10] = -1.0f;
    proj[12] = -1.0f;
    proj[13] =  1.0f;
    proj[15] =  1.0f;

    pglUniformMatrix4fv(uProj, 1, GL_FALSE, proj);
    pglActiveTexture(GL_TEXTURE0);
    pglUniform1i(uTex, 0);

    pglBindVertexArray(vao);
    pglBindBuffer(GL_ARRAY_BUFFER, vbo);
    pglBufferSubData(GL_ARRAY_BUFFER, 0, quadCount * 6 * 8 * sizeof(float), quadData);

    // Replay segments in draw order, re-binding only on texture change:
    // a hotbar of 9 icons + text still costs just a handful of draw calls.
    int first = 0;
    for (int s = 0; s < segCount; s++) {
        pglUniform1i(uRGBA, segs[s].rgba);
        pglBindTexture(GL_TEXTURE_2D, segs[s].tex);
        pglDrawArrays(GL_TRIANGLES, first, segs[s].quads * 6);
        first += segs[s].quads * 6;
    }
    pglBindVertexArray(0);
}

void DebugOverlay::flush() {
    flushBatch();
}

