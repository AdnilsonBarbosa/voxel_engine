#pragma once
// WeatherConfig — centralized tunable parameters for the weather system.
// All values are reasonable defaults; tweak at runtime or from a config file.
#include <cstdint>

namespace WM {

struct WeatherConfig {
    // ── Particle system ──────────────────────────────────────────────────────
    int   maxParticles       = 5000;    // hard cap on particle buffer
    float spawnRateMultiply  = 3.5f;    // global spawn speed multiplier
    float particleLifetime   = 2.5f;    // base lifetime in seconds
    float rainFallSpeed      = 18.0f;   // m/s downward (fast = realistic rain speed)
    float snowFallSpeed      = 1.8f;    // m/s downward
    float rainStreakLength   = 7.0f;    // horizontal compression factor (higher = thinner drop)
    float windInfluence      = 1.0f;    // how much wind affects particles
    float spawnRadius        = 22.0f;   // horizontal spawn radius around camera
    float spawnHeight        = 14.0f;   // above camera to spawn
    float despawnBelow       = -22.0f;  // kill particles this far below camera

    // ── Splash effects ───────────────────────────────────────────────────────
    bool  enableSplashes     = true;    // spawn splash particles on ground hit
    int   maxSplashes        = 256;     // splash particle pool
    float splashLifetime     = 0.4f;    // seconds
    float splashSize         = 2.0f;    // point size

    // ── Rain states (intensity thresholds) ───────────────────────────────────
    float lightRainRate      = 0.15f;   // rainRate below this = light
    float mediumRainRate     = 0.45f;   // rainRate below this = medium
    // above mediumRainRate = heavy

    // ── Environment ──────────────────────────────────────────────────────────
    float fogReduction       = 0.6f;    // fog contracts by this fraction during rain
    float lightDimming       = 0.4f;    // max light reduction during heavy rain

    // ── Mobile optimization ──────────────────────────────────────────────────
    int   mobileMaxParticles = 1500;    // reduced cap for Android
    float updateInterval     = 0.0f;    // 0 = every frame; >0 = throttle updates (seconds)
};

// Global default config (mutable at runtime)
inline WeatherConfig& weatherConfig() {
    static WeatherConfig cfg;
    return cfg;
}

// Classify rain intensity from rate
inline const char* rainIntensityName(float rainRate, const WeatherConfig& cfg) {
    if (rainRate < cfg.lightRainRate)  return "Light";
    if (rainRate < cfg.mediumRainRate) return "Medium";
    return "Heavy";
}

} // namespace WM
