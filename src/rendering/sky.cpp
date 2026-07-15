#include "sky.h"
#include "platform.h"

static const char* SKY_VS = R"(
#version 130
in vec2 aPos;
out vec2 vNdc;
void main() {
    vNdc = aPos;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* SKY_FS = R"(
#version 130
in vec2 vNdc;
out vec4 fragColor;
uniform vec3  uCamRight, uCamUp, uCamFwd;
uniform float uTanX, uTanY, uTime;
uniform vec3  uSunDir;
uniform vec3  uAmbient;
uniform float uCloudCover;
uniform float uLightMult;
uniform float uWindX, uWindZ;     // cloud drift direction
uniform vec3  uCloudColor;        // interpolated by WeatherManager
uniform float uFogDensity;        // atmospheric horizon haze

// ── Noise helpers ────────────────────────────────────────────────────────────
float hash2(vec2 p)  { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float hash3(vec3 p)  { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash2(i),         hash2(i+vec2(1,0)), f.x),
               mix(hash2(i+vec2(0,1)), hash2(i+vec2(1,1)), f.x), f.y);
}

void main() {
    vec3 ray = normalize(uCamFwd + uCamRight*vNdc.x*uTanX + uCamUp*vNdc.y*uTanY);

    // ── Sun elevation and day/night blend ────────────────────────────────────
    float elev   = uSunDir.y;                           // -1 nadir … +1 zenith
    float isDay  = smoothstep(-0.10, 0.12, elev);       // 0=full night, 1=full day
    float isNight= 1.0 - smoothstep(-0.15, 0.05, elev); // 1=deep night, 0=day

    // ── Base sky gradient ─────────────────────────────────────────────────────
    vec3 dayZenith    = vec3(0.18, 0.37, 0.82);
    vec3 dayHorizon   = vec3(0.60, 0.76, 0.94);
    vec3 nightZenith  = vec3(0.00, 0.01, 0.04);
    vec3 nightHorizon = vec3(0.04, 0.05, 0.10);

    vec3 zenith  = mix(nightZenith,  dayZenith,  isDay);
    vec3 horizon = mix(nightHorizon, dayHorizon, isDay);

    float upFactor = pow(max(ray.y, 0.0), 0.45);
    vec3 col = mix(horizon, zenith, upFactor);
    // Ground below horizon: slightly warm haze
    col = mix(col, vec3(0.42, 0.47, 0.50), clamp(-ray.y * 3.5, 0.0, 1.0));

    // ── Sunrise / sunset golden hour ─────────────────────────────────────────
    // Only active when sun is near the horizon (|elev| < 0.25)
    float sunHorizonFac = smoothstep(0.25, 0.0, abs(elev)) * isDay;
    // Direction toward sun projected on XZ plane (ignoring elevation)
    vec3  sunXZ      = normalize(vec3(uSunDir.x, 0.0, uSunDir.z));
    float towardSun  = max(dot(vec3(ray.x, 0.0, ray.z) / max(length(vec3(ray.x,0,ray.z)),0.001), sunXZ), 0.0);

    // Golden corona — near the sun on the horizon
    float corona     = pow(towardSun, 2.5) * sunHorizonFac;
    vec3  goldenCol  = mix(vec3(1.0, 0.52, 0.10), vec3(1.0, 0.75, 0.30),
                           smoothstep(-0.05, 0.12, elev));
    col = mix(col, goldenCol, corona * 0.75 * (1.0 - ray.y * 1.5));

    // Horizon glow ring (warm tint across the whole horizon at golden hour)
    float horizonGlow = sunHorizonFac * (1.0 - abs(ray.y) * 3.0);
    col += vec3(0.30, 0.12, 0.02) * max(0.0, horizonGlow) * 0.4;

    // ── Storm atmosphere dimming ──────────────────────────────────────────────
    col *= uLightMult;

    // ── Sun disc + atmospheric glow ───────────────────────────────────────────
    float sd = max(dot(ray, uSunDir), 0.0);
    col += vec3(1.00, 0.97, 0.85) * pow(sd, 2000.0) * 1.5 * isDay;          // disc
    col += vec3(1.00, 0.85, 0.55) * pow(sd, 12.0)   * 0.28 * isDay * uLightMult; // inner glow
    col += goldenCol               * pow(sd, 5.0)    * 0.10 * sunHorizonFac; // corona burst

    // ── Moon ─────────────────────────────────────────────────────────────────
    float md = max(dot(ray, -uSunDir), 0.0);
    col += vec3(0.88, 0.90, 1.00) * pow(md, 3000.0) * 0.90 * isNight;       // disc
    col += vec3(0.35, 0.38, 0.55) * pow(md, 28.0)   * 0.12 * isNight;       // glow

    // ── Stars (night / twilight) ──────────────────────────────────────────────
    float starVis = isNight * clamp(ray.y * 2.5, 0.0, 1.0);
    if (starVis > 0.005) {
        // Slowly rotate star dome
        float angle  = uTime * 0.000035;
        float cosA   = cos(angle), sinA = sin(angle);
        vec3  sRay   = vec3(ray.x * cosA - ray.z * sinA, ray.y,
                            ray.x * sinA + ray.z * cosA);
        vec3  cellF  = floor(sRay * 130.0);
        float sNoise = hash3(cellF + vec3(5.3, 2.9, 8.1));
        float bright = step(0.992, sNoise);          // ~0.8% of sky cells
        float twinkle= 0.70 + 0.30 * sin(uTime * (1.5 + sNoise * 9.0));
        col += vec3(0.90, 0.93, 1.00) * bright * twinkle * starVis;
        // Occasional brighter star
        float sBright2 = step(0.998, hash3(cellF + vec3(1.1, 7.3, 3.7)));
        col += vec3(1.00, 0.95, 0.85) * sBright2 * twinkle * starVis * 2.0;
    }

    // ── Clouds ───────────────────────────────────────────────────────────────
    if (ray.y > 0.015 && uCloudCover > 0.01) {
        vec2 uv = ray.xz / (ray.y + 0.12);
        // Wind-driven drift: UV offset accumulates over time in wind direction
        vec2 drift = vec2(uWindX, uWindZ) * uTime * 0.007;

        // Two-octave noise for natural cloud shapes
        float c1 = vnoise((uv + drift)        * 1.30);
        float c2 = vnoise((uv + drift * 1.25) * 2.80 + vec2(4.7, 2.3));
        float cl = c1 * 0.65 + c2 * 0.35;

        float thresh  = mix(0.73, 0.25, uCloudCover);
        cl = smoothstep(thresh, thresh + 0.22, cl);
        cl *= (1.0 - clamp(ray.y * 0.55, 0.0, 0.65)); // thin near zenith

        // Cloud lighting: top faces brighter (lit by sun), base darker
        float cloudBase  = mix(0.45, 0.65, isDay);
        float cloudLit   = cloudBase + 0.35 * isDay * uLightMult;
        vec3  cloudCol   = uCloudColor * mix(cloudBase, cloudLit, 0.7);

        float cloudAlpha = cl * mix(0.38, 0.90, uCloudCover);
        col = mix(col, cloudCol, cloudAlpha);
    }

    // ── Atmospheric horizon haze / fog ────────────────────────────────────────
    float hazeY  = clamp(1.0 - ray.y * 4.5, 0.0, 1.0);
    vec3  hazeColor = mix(horizon, vec3(0.72, 0.76, 0.82), uFogDensity * 0.6);
    col = mix(col, hazeColor, hazeY * 0.30 * (1.0 + uFogDensity * 2.0));

    fragColor = vec4(col, 1.0);
}
)";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = pglCreateShader(type);
    pglShaderSource(s, 1, &src, nullptr);
    pglCompileShader(s);
    GLint ok = 0;
    pglGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; pglGetShaderInfoLog(s, sizeof(log), nullptr, log);
        SDL_Log("[Sky] shader error: %s", log);
        pglDeleteShader(s); return 0;
    }
    return s;
}

void Sky::init() {
    GLuint vs = compileShader(GL_VERTEX_SHADER,   SKY_VS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, SKY_FS);
    if (!vs || !fs) return;

    program = pglCreateProgram();
    pglAttachShader(program, vs);
    pglAttachShader(program, fs);
    pglLinkProgram(program);
    GLint ok = 0;
    pglGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; pglGetProgramInfoLog(program, sizeof(log), nullptr, log);
        SDL_Log("[Sky] link error: %s", log);
        pglDeleteProgram(program); program = 0;
    }
    pglDeleteShader(vs);
    pglDeleteShader(fs);
    if (!program) return;

    aPos         = pglGetAttribLocation(program,  "aPos");
    uCamRight    = pglGetUniformLocation(program, "uCamRight");
    uCamUp       = pglGetUniformLocation(program, "uCamUp");
    uCamFwd      = pglGetUniformLocation(program, "uCamFwd");
    uTanX        = pglGetUniformLocation(program, "uTanX");
    uTanY        = pglGetUniformLocation(program, "uTanY");
    uTime        = pglGetUniformLocation(program, "uTime");
    uSunDir      = pglGetUniformLocation(program, "uSunDir");
    uAmbient     = pglGetUniformLocation(program, "uAmbient");
    uCloudCover  = pglGetUniformLocation(program, "uCloudCover");
    uLightMult   = pglGetUniformLocation(program, "uLightMult");
    uWindX       = pglGetUniformLocation(program, "uWindX");
    uWindZ       = pglGetUniformLocation(program, "uWindZ");
    uCloudColor  = pglGetUniformLocation(program, "uCloudColor");
    uFogDensity  = pglGetUniformLocation(program, "uFogDensity");

    const float verts[6] = { -1.f,-1.f,  3.f,-1.f,  -1.f,3.f };
    pglGenVertexArrays(1, &vao);
    pglGenBuffers(1, &vbo);
    pglBindVertexArray(vao);
    pglBindBuffer(GL_ARRAY_BUFFER, vbo);
    pglBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    pglEnableVertexAttribArray((GLuint)aPos);
    pglVertexAttribPointer((GLuint)aPos, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    pglBindVertexArray(0);
}

void Sky::render(const float camRight[3], const float camUp[3], const float camFwd[3],
                 float tanX, float tanY, const float sunDir[3], float time,
                 const float ambient[3],
                 float cloudCover, float lightMult,
                 float windX, float windZ,
                 const float cloudRGB[3], float fogDensity)
{
    if (!program) return;

    static const float DEFAULT_CLOUD[3] = { 1.0f, 1.0f, 1.0f };
    const float* cld = cloudRGB ? cloudRGB : DEFAULT_CLOUD;

    pglDisable(GL_DEPTH_TEST);
    pglUseProgram(program);
    pglUniform3f(uCamRight,   camRight[0], camRight[1], camRight[2]);
    pglUniform3f(uCamUp,      camUp[0],    camUp[1],    camUp[2]);
    pglUniform3f(uCamFwd,     camFwd[0],   camFwd[1],   camFwd[2]);
    pglUniform1f(uTanX,       tanX);
    pglUniform1f(uTanY,       tanY);
    pglUniform1f(uTime,       time);
    pglUniform3f(uSunDir,     sunDir[0],   sunDir[1],   sunDir[2]);
    pglUniform3f(uAmbient,    ambient[0],  ambient[1],  ambient[2]);
    pglUniform1f(uCloudCover, cloudCover);
    pglUniform1f(uLightMult,  lightMult);
    pglUniform1f(uWindX,      windX);
    pglUniform1f(uWindZ,      windZ);
    pglUniform3f(uCloudColor, cld[0], cld[1], cld[2]);
    pglUniform1f(uFogDensity, fogDensity);

    pglBindVertexArray(vao);
    pglDrawArrays(GL_TRIANGLES, 0, 3);
    pglBindVertexArray(0);
    pglEnable(GL_DEPTH_TEST);
}

void Sky::cleanup() {
    if (vbo)     { pglDeleteBuffers(1, &vbo);      vbo = 0; }
    if (vao)     { pglDeleteVertexArrays(1, &vao); vao = 0; }
    if (program) { pglDeleteProgram(program);      program = 0; }
}
