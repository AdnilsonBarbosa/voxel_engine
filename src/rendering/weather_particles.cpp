#include "weather_particles.h"
#include "platform.h"
#include <cstdlib>
#include <cmath>
#include <cstring>

// ══════════════════════════════════════════════════════════════════════════════
// RAIN / SNOW SHADER — renders particles as stretched points
// ══════════════════════════════════════════════════════════════════════════════

static const char* RAIN_VS = R"(
#version 130
in vec3 aPos;
in float aSize;
in float aAlpha;
in float aStretch;
uniform mat4 uMVP;
uniform float uTime;
uniform float uPointScale;
out float vAlpha;
out float vStretch;
void main() {
    vec4 clip = uMVP * vec4(aPos, 1.0);
    gl_Position = clip;
    float dist = max(clip.w, 0.1);
    float base = aSize * uPointScale / dist;
    gl_PointSize = clamp(base, 2.0, 30.0);
    vAlpha = aAlpha;
    vStretch = aStretch;
}
)";

static const char* RAIN_FS = R"(
#version 130
in float vAlpha;
in float vStretch;
out vec4 fragColor;
void main() {
    vec2 pc = gl_PointCoord * 2.0 - 1.0;
    // Compress HORIZONTALLY → tall, thin vertical rain streak.
    // vStretch=8 means width is 1/8 of height — a genuine rain drop, not a blob.
    float d = (pc.x * vStretch) * (pc.x * vStretch) + pc.y * pc.y;
    if (d > 1.0) discard;
    float edge = 1.0 - d;
    // Blue-gray, slightly translucent. Brighter at center, fade at edges.
    vec3 col = vec3(0.48, 0.57, 0.74) + vec3(0.12, 0.12, 0.14) * edge;
    fragColor = vec4(col, vAlpha * edge * 0.88);
}
)";

// ══════════════════════════════════════════════════════════════════════════════
// SPLASH SHADER — small bright dots that appear when rain hits ground
// ══════════════════════════════════════════════════════════════════════════════

static const char* SPLASH_VS = R"(
#version 130
in vec3 aPos;
in float aSize;
in float aAlpha;
uniform mat4 uMVP;
uniform float uPointScale;
out float vAlpha;
void main() {
    vec4 clip = uMVP * vec4(aPos, 1.0);
    gl_Position = clip;
    float dist = max(clip.w, 0.1);
    gl_PointSize = clamp(aSize * uPointScale / dist, 1.0, 5.0);
    vAlpha = aAlpha;
}
)";

static const char* SPLASH_FS = R"(
#version 130
in float vAlpha;
out vec4 fragColor;
void main() {
    vec2 pc = gl_PointCoord * 2.0 - 1.0;
    float d = dot(pc, pc);
    if (d > 0.8) discard;
    float a = vAlpha * (1.0 - d * 1.25);
    fragColor = vec4(0.85, 0.9, 1.0, a * 0.6);
}
)";

// ── Shader compilation helper ────────────────────────────────────────────────

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = pglCreateShader(type);
    pglShaderSource(s, 1, &src, nullptr);
    pglCompileShader(s);
    GLint ok = 0;
    pglGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; pglGetShaderInfoLog(s, sizeof(log), nullptr, log);
        SDL_Log("[WeatherParticles] shader error: %s", log);
        pglDeleteShader(s); return 0;
    }
    return s;
}

static bool linkProgram(GLuint program) {
    pglLinkProgram(program);
    GLint ok = 0;
    pglGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; pglGetProgramInfoLog(program, sizeof(log), nullptr, log);
        SDL_Log("[WeatherParticles] link error: %s", log);
        pglDeleteProgram(program);
        return false;
    }
    return true;
}

// ── Fast LCG (no std::rand overhead) ────────────────────────────────────────

static uint32_t s_rng = 12345u;
static inline float fastRand() {
    s_rng = s_rng * 1664525u + 1013904223u;
    return (float)(s_rng >> 8) / (float)(1 << 24);
}

// ══════════════════════════════════════════════════════════════════════════════
// INIT
// ══════════════════════════════════════════════════════════════════════════════

void WeatherParticles::init(int maxCount) {
    const auto& cfg = WM::weatherConfig();
    maxRain_     = maxCount;
    maxSplashes_ = cfg.maxSplashes;

    rain_     = new RainParticle[maxRain_];
    splashes_ = new SplashParticle[maxSplashes_];
    memset(rain_,     0, maxRain_     * sizeof(RainParticle));
    memset(splashes_, 0, maxSplashes_ * sizeof(SplashParticle));

    // Required so gl_PointSize in vertex shader actually controls point size.
    // Without this, all GL_POINTS render as 1px regardless of gl_PointSize writes.
    pglEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    // Required for gl_PointCoord to work in fragment shader (OpenGL compat profile).
    pglEnable(GL_POINT_SPRITE);

    compileRainShader_();
    compileSplashShader_();
}

void WeatherParticles::compileRainShader_() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, RAIN_VS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, RAIN_FS);
    if (!vs || !fs) return;

    rainProgram = pglCreateProgram();
    pglAttachShader(rainProgram, vs);
    pglAttachShader(rainProgram, fs);
    if (!linkProgram(rainProgram)) { rainProgram = 0; return; }
    pglDeleteShader(vs);
    pglDeleteShader(fs);

    rPos   = pglGetAttribLocation(rainProgram, "aPos");
    rSize  = pglGetAttribLocation(rainProgram, "aSize");
    rAlpha = pglGetAttribLocation(rainProgram, "aAlpha");
    rStretch = pglGetAttribLocation(rainProgram, "aStretch");
    rMVP       = pglGetUniformLocation(rainProgram, "uMVP");
    rTime      = pglGetUniformLocation(rainProgram, "uTime");
    rPointScale = pglGetUniformLocation(rainProgram, "uPointScale");

    pglGenVertexArrays(1, &rainVAO);
    pglGenBuffers(1, &rainVBO);
    pglBindVertexArray(rainVAO);
    pglBindBuffer(GL_ARRAY_BUFFER, rainVBO);
    // Vertex: pos(3) + size(1) + alpha(1) + stretch(1) = 6 floats
    pglBufferData(GL_ARRAY_BUFFER, maxRain_ * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    int stride = 6 * sizeof(float);
    pglEnableVertexAttribArray((GLuint)rPos);
    pglVertexAttribPointer((GLuint)rPos,   3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    pglEnableVertexAttribArray((GLuint)rSize);
    pglVertexAttribPointer((GLuint)rSize,  1, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
    pglEnableVertexAttribArray((GLuint)rAlpha);
    pglVertexAttribPointer((GLuint)rAlpha, 1, GL_FLOAT, GL_FALSE, stride, (void*)(4*sizeof(float)));
    pglEnableVertexAttribArray((GLuint)rStretch);
    pglVertexAttribPointer((GLuint)rStretch, 1, GL_FLOAT, GL_FALSE, stride, (void*)(5*sizeof(float)));

    pglBindVertexArray(0);
}

void WeatherParticles::compileSplashShader_() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, SPLASH_VS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, SPLASH_FS);
    if (!vs || !fs) return;

    splashProgram = pglCreateProgram();
    pglAttachShader(splashProgram, vs);
    pglAttachShader(splashProgram, fs);
    if (!linkProgram(splashProgram)) { splashProgram = 0; return; }
    pglDeleteShader(vs);
    pglDeleteShader(fs);

    sPos   = pglGetAttribLocation(splashProgram, "aPos");
    sSize  = pglGetAttribLocation(splashProgram, "aSize");
    sAlpha = pglGetAttribLocation(splashProgram, "aAlpha");
    sMVP       = pglGetUniformLocation(splashProgram, "uMVP");
    sPointScale = pglGetUniformLocation(splashProgram, "uPointScale");

    pglGenVertexArrays(1, &splashVAO);
    pglGenBuffers(1, &splashVBO);
    pglBindVertexArray(splashVAO);
    pglBindBuffer(GL_ARRAY_BUFFER, splashVBO);
    // Vertex: pos(3) + size(1) + alpha(1) = 5 floats
    pglBufferData(GL_ARRAY_BUFFER, maxSplashes_ * 5 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    int stride = 5 * sizeof(float);
    pglEnableVertexAttribArray((GLuint)sPos);
    pglVertexAttribPointer((GLuint)sPos,   3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    pglEnableVertexAttribArray((GLuint)sSize);
    pglVertexAttribPointer((GLuint)sSize,  1, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
    pglEnableVertexAttribArray((GLuint)sAlpha);
    pglVertexAttribPointer((GLuint)sAlpha, 1, GL_FLOAT, GL_FALSE, stride, (void*)(4*sizeof(float)));

    pglBindVertexArray(0);
}

// ══════════════════════════════════════════════════════════════════════════════
// CLEANUP
// ══════════════════════════════════════════════════════════════════════════════

void WeatherParticles::cleanup() {
    delete[] rain_;     rain_     = nullptr;
    delete[] splashes_; splashes_ = nullptr;

    if (rainVBO)     { pglDeleteBuffers(1, &rainVBO);      rainVBO = 0; }
    if (rainVAO)     { pglDeleteVertexArrays(1, &rainVAO);  rainVAO = 0; }
    if (rainProgram) { pglDeleteProgram(rainProgram);       rainProgram = 0; }

    if (splashVBO)     { pglDeleteBuffers(1, &splashVBO);      splashVBO = 0; }
    if (splashVAO)     { pglDeleteVertexArrays(1, &splashVAO);  splashVAO = 0; }
    if (splashProgram) { pglDeleteProgram(splashProgram);       splashProgram = 0; }
}

// ══════════════════════════════════════════════════════════════════════════════
// PARTICLE SPAWNING
// ══════════════════════════════════════════════════════════════════════════════

void WeatherParticles::respawnRain_(RainParticle& p, float cx, float cy, float cz,
                                    float fx, float fy, float fz, float wind,
                                    bool snow, const WM::WeatherConfig& cfg) {
    float spreadX = (fastRand() * 2.0f - 1.0f) * cfg.spawnRadius;
    float spreadZ = (fastRand() * 2.0f - 1.0f) * cfg.spawnRadius;
    float ahead   = 5.0f + fastRand() * 15.0f;

    p.x = cx + fx * ahead + spreadX;
    // Spawn above AND at camera level — ensures particles are visible
    p.y = cy + 5.0f + fastRand() * 15.0f;
    p.z = cz + fz * ahead + spreadZ;

    if (snow) {
        p.vy = -(cfg.snowFallSpeed + fastRand() * 1.5f);
        p.vx = (wind - 0.5f) * cfg.windInfluence * 2.0f
             + (fastRand() - 0.5f) * 0.6f;
        p.vz = (fastRand() - 0.5f) * 0.4f;
        p.size    = 3.0f + fastRand() * 3.0f;
        p.stretch = 1.0f;  // snow = round
    } else {
        p.vy = -(cfg.rainFallSpeed + fastRand() * 6.0f);   // 18-24 m/s
        p.vx = (wind - 0.3f) * cfg.windInfluence * 6.0f
             + (fastRand() - 0.5f) * 0.5f;
        p.vz = (fastRand() - 0.5f) * 0.3f;
        p.size    = 1.8f + fastRand() * 1.4f;              // 1.8-3.2 (visible thin drop)
        p.stretch = cfg.rainStreakLength + fastRand() * 2.0f; // 7-9x narrow
    }

    p.life    = cfg.particleLifetime + fastRand() * 1.5f;
    p.maxLife = p.life;
}

void WeatherParticles::spawnSplash_(float wx, float wy, float wz,
                                    const WM::WeatherConfig& cfg) {
    if (splashCount_ >= maxSplashes_) return;
    SplashParticle& sp = splashes_[splashCount_++];

    sp.x = wx;
    sp.y = wy;
    sp.z = wz;
    // Outward splash velocity (mostly upward, slight random spread)
    float angle = fastRand() * 6.2832f;
    float force = 1.0f + fastRand() * 2.0f;
    sp.vx = cosf(angle) * force * 0.3f;
    sp.vy = 1.5f + fastRand() * 2.0f;  // pop upward
    sp.vz = sinf(angle) * force * 0.3f;
    sp.life    = cfg.splashLifetime;
    sp.maxLife = cfg.splashLifetime;
    sp.size    = cfg.splashSize;
}

// ══════════════════════════════════════════════════════════════════════════════
// UPDATE
// ══════════════════════════════════════════════════════════════════════════════

void WeatherParticles::update(float dt, float rainRate, float snowRate, float windSpeed,
                               const float camPos[3], const float camFwd[3]) {
    if (!rain_) return;

    const auto& cfg = WM::weatherConfig();

    // Mobile throttle: skip updates if interval hasn't elapsed
    if (cfg.updateInterval > 0.0f) {
        updateTimer_ += dt;
        if (updateTimer_ < cfg.updateInterval) return;
        dt = updateTimer_;  // use accumulated dt
        updateTimer_ = 0.0f;
    }

    bool isSnow = (snowRate > rainRate);
    float rate  = fmaxf(rainRate, snowRate) * cfg.spawnRateMultiply;

    // On mobile, reduce maxRain_ via init(cfg.mobileMaxParticles) instead of here.
    // Desktop always uses the full buffer so rain is dense enough to be visible.
    int hardCap    = maxRain_;
    int targetCount = (int)(rate * (float)hardCap * 0.7f);

    // ── Spawn rain particles ─────────────────────────────────────────────────
    if (rainCount_ < targetCount) {
        float spawnPerFrame = (float)(targetCount - rainCount_) * dt * 10.0f;
        spawnAccum_ += spawnPerFrame;
        while (spawnAccum_ >= 1.0f && rainCount_ < maxRain_) {
            respawnRain_(rain_[rainCount_],
                        camPos[0], camPos[1], camPos[2],
                        camFwd[0], camFwd[1], camFwd[2],
                        windSpeed, isSnow, cfg);
            rainCount_++;
            spawnAccum_ -= 1.0f;
        }
    }

    // ── Update rain particles + detect ground hits for splashes ──────────────
    int alive = 0;
    for (int i = 0; i < rainCount_; i++) {
        RainParticle& p = rain_[i];
        p.life -= dt;

        if (p.life <= 0.0f) {
            if (alive < targetCount) {
                respawnRain_(p,
                            camPos[0], camPos[1], camPos[2],
                            camFwd[0], camFwd[1], camFwd[2],
                            windSpeed, isSnow, cfg);
            } else {
                continue;
            }
        }

        // Integrate
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;

        // Kill if too far below camera or too far horizontally
        float dx = p.x - camPos[0];
        float dz = p.z - camPos[2];
        bool tooFar  = (p.y < camPos[1] + cfg.despawnBelow);
        bool tooWide = (dx*dx + dz*dz > cfg.spawnRadius * cfg.spawnRadius * 4.0f);

        if (tooFar || tooWide) {
            // Spawn splash at the position where rain "hit"
            if (cfg.enableSplashes && !isSnow && p.y < camPos[1] + 2.0f) {
                spawnSplash_(p.x, camPos[1] - 1.0f, p.z, cfg);
            }
            if (alive < targetCount) {
                respawnRain_(p,
                            camPos[0], camPos[1], camPos[2],
                            camFwd[0], camFwd[1], camFwd[2],
                            windSpeed, isSnow, cfg);
            } else {
                continue;
            }
        }

        if (alive != i) rain_[alive] = p;
        alive++;
    }
    rainCount_ = alive;

    // ── Update splash particles ──────────────────────────────────────────────
    int splashAlive = 0;
    for (int i = 0; i < splashCount_; i++) {
        SplashParticle& sp = splashes_[i];
        sp.life -= dt;
        if (sp.life <= 0.0f) continue;

        sp.x += sp.vx * dt;
        sp.y += sp.vy * dt;
        sp.z += sp.vz * dt;
        sp.vy -= 8.0f * dt;  // gravity on splash droplets

        if (splashAlive != i) splashes_[splashAlive] = sp;
        splashAlive++;
    }
    splashCount_ = splashAlive;
}

// ══════════════════════════════════════════════════════════════════════════════
// DRAW
// ══════════════════════════════════════════════════════════════════════════════

void WeatherParticles::draw(const float* mvp, float time) {
    // Debug: log particle count every 60 frames
    static int frameCounter = 0;
    if (++frameCounter % 60 == 0 && rainCount_ > 0) {
        SDL_Log("[RainParticles] drawing %d drops, %d splashes", rainCount_, splashCount_);
    }

    if (rainCount_ == 0 && splashCount_ == 0) return;

    // GL state: keep depth TEST so rain is occluded by nearby walls/terrain,
    // but disable depth WRITE so rain doesn't z-fight with world geometry.
    pglDepthMask(GL_FALSE);
    pglEnable(GL_BLEND);
    pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ── Draw rain/snow streaks ───────────────────────────────────────────────
    if (rainProgram && rainCount_ > 0) {
        // Interleaved: pos(3) + size(1) + alpha(1) + stretch(1) = 6 floats
        static const int STACK_MAX = 2048;
        float stackBuf[STACK_MAX * 6];
        float* buf = (rainCount_ * 6 <= STACK_MAX) ? stackBuf
                   : new float[rainCount_ * 6];

        for (int i = 0; i < rainCount_; i++) {
            const RainParticle& p = rain_[i];
            float t = p.life / p.maxLife;
            float alpha = (t > 0.9f) ? (1.0f - t) * 10.0f     // fade in (fast)
                        : (t < 0.1f) ? t * 10.0f               // fade out (fast)
                        : 0.85f;  // semi-transparent: visible but not opaque

            int o = i * 6;
            buf[o]   = p.x;
            buf[o+1] = p.y;
            buf[o+2] = p.z;
            buf[o+3] = p.size;
            buf[o+4] = alpha;
            buf[o+5] = p.stretch;
        }

        pglUseProgram(rainProgram);
        pglUniformMatrix4fv(rMVP, 1, GL_FALSE, mvp);
        pglUniform1f(rTime, time);
        pglUniform1f(rPointScale, 220.0f);   // ~3px wide × 25px tall at 10m distance

        pglBindVertexArray(rainVAO);
        pglBindBuffer(GL_ARRAY_BUFFER, rainVBO);
        pglBufferSubData(GL_ARRAY_BUFFER, 0, rainCount_ * 6 * sizeof(float), buf);
        pglDrawArrays(GL_POINTS, 0, rainCount_);
        pglBindVertexArray(0);

        if (buf != stackBuf) delete[] buf;
    }

    // ── Draw splash particles ────────────────────────────────────────────────
    if (splashProgram && splashCount_ > 0) {
        float buf[256 * 5];  // max 256 splashes, always fits
        int count = splashCount_;
        if (count > 256) count = 256;

        for (int i = 0; i < count; i++) {
            const SplashParticle& sp = splashes_[i];
            float t = sp.life / sp.maxLife;
            float alpha = t * 0.8f;

            int o = i * 5;
            buf[o]   = sp.x;
            buf[o+1] = sp.y;
            buf[o+2] = sp.z;
            buf[o+3] = sp.size * t;  // shrink as they fade
            buf[o+4] = alpha;
        }

        pglUseProgram(splashProgram);
        pglUniformMatrix4fv(sMVP, 1, GL_FALSE, mvp);
        pglUniform1f(sPointScale, 800.0f);

        pglBindVertexArray(splashVAO);
        pglBindBuffer(GL_ARRAY_BUFFER, splashVBO);
        pglBufferSubData(GL_ARRAY_BUFFER, 0, count * 5 * sizeof(float), buf);
        pglDrawArrays(GL_POINTS, 0, count);
        pglBindVertexArray(0);
    }

    // ── Restore GL state ─────────────────────────────────────────────────────
    pglDepthMask(GL_TRUE);
    pglDisable(GL_BLEND);
}
