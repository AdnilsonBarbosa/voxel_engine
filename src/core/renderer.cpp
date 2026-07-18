#include "renderer.h"
#include "platform.h"
#include <cstdio>

static const char* VS_SRC = R"(
#version 130
in vec3  aPosition;
in vec2  aUV;
in vec2  aTile;
in float aLight;
in float aSky;
in float aBlock;
in float aWave;
out vec2  vUV;
out vec2  vTile;
out float vLight;
out float vSky;
out float vBlock;
out float vDist;
uniform mat4  uMVP;
uniform float uTime;
uniform vec3  uCamPos;
void main() {
    vUV    = aUV;
    vTile  = aTile;
    vLight = aLight;
    vSky   = aSky;
    vBlock = aBlock;
    vec3 p = aPosition;
    // Gentle sway for water surfaces / plants (aWave weights the effect)
    p.y += sin(uTime * 1.5 + p.x * 0.6 + p.z * 0.6) * 0.08 * aWave;
    vDist = length(p - uCamPos);
    gl_Position = uMVP * vec4(p, 1.0);
}
)";

static const char* FS_SRC = R"(
#version 130
in vec2  vUV;
in vec2  vTile;
in float vLight;
in float vSky;
in float vBlock;
in float vDist;
out vec4 fragColor;
uniform sampler2D uAtlas;
uniform float uTileSize;   // normalised tile size in the atlas
uniform float uTexel;      // 1 / atlasPixels
uniform vec3  uAmbient;    // biome environment colour
uniform float uFogStart;
uniform float uFogEnd;
uniform float uSunLight;   // day/night sky brightness (0.35 night .. 1.0 noon)
uniform float uCaveAmbient;// minimum light deep underground
void main() {
    // Wrap the (possibly >1) UV into this tile's atlas sub-rect. Half-texel
    // inset avoids bleeding into neighbouring tiles under NEAREST filtering.
    vec2 f  = fract(vUV);
    vec2 uv = vTile + uTexel * 0.5 + f * (uTileSize - uTexel);
    vec4 tex = texture(uAtlas, uv);
    if (tex.a < 0.5) discard;                         // alpha cutout (leaves/plants)

    // Sky exposure drives day/night; torches/emissive blocks add local light
    // that stays bright even in dark caves. Take the brighter of the two.
    float sky = mix(uCaveAmbient, uSunLight, vSky);
    float env = max(sky, vBlock);
    vec3 col = tex.rgb * vLight * env;                // texture Ã— (dir+AO) Ã— light
    // Gentle filmic-ish lift: deepens shadows, saves highlights from clipping.
    col = col * (1.05 + 0.25 * col) / (1.0 + 0.30 * col);
    float fog = clamp((vDist - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
    // Semi-transparent texels (water) keep their alpha â†’ seabed shows through.
    fragColor = vec4(mix(col, uAmbient, fog), tex.a);
}
)";

#ifdef __ANDROID__
static const char* VS_SRC_GLES = R"(
#version 100
precision highp float;
attribute vec3 aPosition;
attribute vec2 aUV;
attribute vec2 aTile;
attribute float aLight;
attribute float aSky;
attribute float aBlock;
attribute float aWave;
varying vec2 vUV;
varying vec2 vTile;
varying float vLight;
varying float vSky;
varying float vBlock;
varying float vDist;
uniform mat4 uMVP;
uniform float uTime;
uniform vec3 uCamPos;
void main() {
    vUV = aUV;
    vTile = aTile;
    vLight = aLight;
    vSky = aSky;
    vBlock = aBlock;
    vec3 p = aPosition;
    p.y += sin(uTime * 1.5 + p.x * 0.6 + p.z * 0.6) * 0.08 * aWave;
    vDist = distance(p, uCamPos);
    gl_Position = uMVP * vec4(p, 1.0);
}
)";

static const char* FS_SRC_GLES = R"(
#version 100
precision mediump float;
varying vec2 vUV;
varying vec2 vTile;
varying float vLight;
varying float vSky;
varying float vBlock;
varying float vDist;
uniform sampler2D uAtlas;
uniform float uTileSize;
uniform float uTexel;
uniform vec3 uAmbient;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uSunLight;
uniform float uCaveAmbient;
void main() {
    vec2 f = fract(vUV);
    vec2 uv = vTile + uTexel * 0.5 + f * (uTileSize - uTexel);
    vec4 tex = texture2D(uAtlas, uv);
    if (tex.a < 0.5) discard;
    float sky = mix(uCaveAmbient, uSunLight, vSky);
    float env = max(sky, vBlock);
    vec3 col = tex.rgb * vLight * env;
    col = col * (1.05 + 0.25 * col) / (1.0 + 0.30 * col);
    float fog = clamp((vDist - uFogStart) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
    gl_FragColor = vec4(mix(col, uAmbient, fog), tex.a);
}
)";
#endif

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = pglCreateShader(type);
    pglShaderSource(s, 1, &src, nullptr);
    pglCompileShader(s);
    GLint ok = 0;
    pglGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        pglGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        SDL_Log("Shader compile error: %s", buf);
        pglDeleteShader(s);
        return 0;
    }
    return s;
}

void Renderer::init() {
    pglClearColor(0.5f, 0.7f, 1.0f, 1.0f); // sky blue
    pglEnable(GL_DEPTH_TEST);
    pglDepthFunc(GL_LEQUAL);

#ifdef __ANDROID__
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   VS_SRC_GLES);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FS_SRC_GLES);
#else
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   VS_SRC);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FS_SRC);
#endif
    if (!vs || !fs) { SDL_Log("Shader compilation failed!"); return; }

    program = pglCreateProgram();
    pglAttachShader(program, vs);
    pglAttachShader(program, fs);
    pglLinkProgram(program);
    GLint ok = 0;
    pglGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        pglGetProgramInfoLog(program, sizeof(buf), nullptr, buf);
        SDL_Log("Program link error: %s", buf);
        pglDeleteProgram(program);
        program = 0;
    }
    pglDeleteShader(vs);
    pglDeleteShader(fs);

    if (!program) return;

    locPos    = pglGetAttribLocation(program,  "aPosition");
    locUV     = pglGetAttribLocation(program,  "aUV");
    locTile   = pglGetAttribLocation(program,  "aTile");
    locLight  = pglGetAttribLocation(program,  "aLight");
    locSky    = pglGetAttribLocation(program,  "aSky");
    locBlock  = pglGetAttribLocation(program,  "aBlock");
    locWave   = pglGetAttribLocation(program,  "aWave");
    uMVP      = pglGetUniformLocation(program, "uMVP");
    uTime     = pglGetUniformLocation(program, "uTime");
    uCamPos   = pglGetUniformLocation(program, "uCamPos");
    uAmbient  = pglGetUniformLocation(program, "uAmbient");
    uFogStart = pglGetUniformLocation(program, "uFogStart");
    uFogEnd   = pglGetUniformLocation(program, "uFogEnd");
    uAtlas    = pglGetUniformLocation(program, "uAtlas");
    uTileSize = pglGetUniformLocation(program, "uTileSize");
    uTexel    = pglGetUniformLocation(program, "uTexel");
    uSunLight   = pglGetUniformLocation(program, "uSunLight");
    uCaveAmbient= pglGetUniformLocation(program, "uCaveAmbient");

    SDL_Log("[Renderer] pos=%d uv=%d tile=%d light=%d sky=%d wave=%d atlas=%d",
            locPos, locUV, locTile, locLight, locSky, locWave, uAtlas);
}

void Renderer::beginFrame() {
    pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // World pass blends: opaque texels are alpha=1 (no cost), water texels are
    // translucent so the seabed shows through. Depth writes stay on.
    pglEnable(GL_BLEND);
    pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::setAtlas(GLuint tex, float tileSizeUV, float texelUV) {
    atlasTex = tex;
    tileSize = tileSizeUV;
    texel    = texelUV;
}

void Renderer::setFrame(const float* mvp, float time,
                        const float camPos[3], const float ambient[3],
                        float fogStart, float fogEnd) {
    if (!program) return;
    pglUseProgram(program);
    pglUniformMatrix4fv(uMVP, 1, GL_FALSE, mvp);
    pglUniform1f(uTime, time);
    pglUniform3f(uCamPos, camPos[0], camPos[1], camPos[2]);
    pglUniform3f(uAmbient, ambient[0], ambient[1], ambient[2]);
    pglUniform1f(uFogStart, fogStart);
    pglUniform1f(uFogEnd, fogEnd);
    pglUniform1f(uTileSize, tileSize);
    pglUniform1f(uTexel, texel);
    pglUniform1f(uSunLight, sunL);
    pglUniform1f(uCaveAmbient, caveA);

    pglActiveTexture(GL_TEXTURE0);
    pglBindTexture(GL_TEXTURE_2D, atlasTex);
    pglUniform1i(uAtlas, 0);
}

void Renderer::cleanup() {
    if (program) { pglDeleteProgram(program); program = 0; }
}



