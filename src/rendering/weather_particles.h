#pragma once
// WeatherParticles — GPU particle system for rain and snow.
//
// Features:
//   • Rain rendered as elongated streaks (not dots) via geometry stretch
//   • Splash particles when rain hits the ground
//   • Snow rendered as soft drifting flakes
//   • Wind affects particle drift
//   • Follows the player — particles only exist in a box around the camera
//   • Mobile-friendly: pool-based, zero per-frame heap allocation
//   • Configurable via WeatherConfig
#include "gl_ext.h"
#include "../weather/weather_defs.h"
#include "../weather/weather_config.h"

class WeatherParticles {
public:
    void init(int maxCount = 3000);
    void cleanup();

    // Update particle positions. Call every frame.
    // dt         = frame delta (seconds)
    // rainRate   = 0..1 from WeatherState
    // snowRate   = 0..1
    // windSpeed  = 0..1
    // camPos[3]  = world-space camera position
    // camFwd[3]  = camera forward direction (normalized)
    void update(float dt, float rainRate, float snowRate, float windSpeed,
                const float camPos[3], const float camFwd[3]);

    // Draw all particles (rain streaks + snow flakes + splashes).
    // Call after world render, before UI overlay.
    void draw(const float* mvp, float time);

    int  activeCount()   const { return rainCount_; }
    int  splashCount()   const { return splashCount_; }
    bool isRaining()     const { return rainCount_ > 0; }

private:
    // ── Particle types ───────────────────────────────────────────────────────
    struct RainParticle {
        float x, y, z;      // world position
        float vx, vy, vz;   // velocity
        float life;          // remaining lifetime
        float maxLife;       // initial lifetime (for fade)
        float size;          // base point size
        float stretch;       // elongation factor (rain=high, snow=1)
    };

    struct SplashParticle {
        float x, y, z;      // world position
        float vx, vy, vz;   // outward velocity (short-lived)
        float life;
        float maxLife;
        float size;
    };

    // ── GL resources ─────────────────────────────────────────────────────────
    GLuint rainProgram = 0, rainVAO = 0, rainVBO = 0;
    GLint  rPos = -1, rSize = -1, rAlpha = -1, rStretch = -1;
    GLint  rMVP = -1, rTime = -1, rPointScale = -1;

    GLuint splashProgram = 0, splashVAO = 0, splashVBO = 0;
    GLint  sPos = -1, sSize = -1, sAlpha = -1;
    GLint  sMVP = -1, sTime = -1, sPointScale = -1;

    // ── Particle pools ───────────────────────────────────────────────────────
    RainParticle*   rain_     = nullptr;
    int             maxRain_  = 0;
    int             rainCount_ = 0;

    SplashParticle* splashes_    = nullptr;
    int             maxSplashes_ = 0;
    int             splashCount_ = 0;

    float spawnAccum_   = 0.0f;
    float splashAccum_  = 0.0f;
    float updateTimer_  = 0.0f;   // for throttled updates on mobile

    // ── Internal helpers ─────────────────────────────────────────────────────
    void respawnRain_(RainParticle& p, float cx, float cy, float cz,
                      float fx, float fy, float fz, float wind, bool snow,
                      const WM::WeatherConfig& cfg);
    void spawnSplash_(float wx, float wy, float wz, const WM::WeatherConfig& cfg);
    void compileRainShader_();
    void compileSplashShader_();
};
