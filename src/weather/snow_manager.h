#pragma once
// SnowManager — snow particles + world snow accumulation state.
// World snow (blocks covered, lake ice) is tracked but visual application
// is deferred to chunk meshing — no per-frame GPU work here.
//
// Accumulation model:
//   snowLevel_ grows during Snow/Blizzard, melts when temperature > 2°C.
//   Range 0-1: 0=bare ground, 1=full snow cover.
//
#include "weather_defs.h"
#include "wind_manager.h"
#include <cmath>

namespace WM {

struct SnowVisuals {
    float spawnRate   = 1.0f;
    float flakeSize   = 3.5f;   // larger than rain drops
    float stretch     = 1.0f;   // snowflakes = near-round
    float fallSpeed   = 1.8f;   // m/s — slow, drifting
    float drift       = 0.6f;   // lateral wind influence
    float alpha       = 0.75f;
};

class SnowManager {
public:
    void update(float dt, const WeatherState& ws, const WindManager& wind) {
        float snowRate = ws.snowRate;
        active_ = (snowRate > 0.01f);
        if (!active_) {
            // Melt when temperature rises above 2°C
            if (ws.temperature > 2.0f && snowLevel_ > 0.0f) {
                float meltRate = (ws.temperature - 2.0f) * 0.0001f; // very slow melt
                snowLevel_ = fmaxf(0.0f, snowLevel_ - meltRate * dt);
            }
            return;
        }

        bool blizzard = (ws.type == WeatherType::Blizzard);
        intensity_ = snowRate * ws.intensity;

        vis_.spawnRate  = blizzard ? 3.5f : 1.8f;
        vis_.flakeSize  = blizzard ? 2.8f : 3.5f;  // blizzard = smaller, denser
        vis_.stretch    = blizzard ? 2.5f : 1.0f;   // blizzard streaks slightly
        vis_.fallSpeed  = blizzard ? 6.0f : 1.8f;
        vis_.drift      = blizzard ? wind.particleX() * 2.0f : wind.particleX() * 0.5f;
        vis_.alpha      = blizzard ? 0.80f : 0.72f;

        // Accumulate snow — 1 full game-hour of Snow ≈ 0.05 level
        float accumulateRate = intensity_ * 0.00001f; // per second
        snowLevel_ = fminf(1.0f, snowLevel_ + accumulateRate * dt);
    }

    bool  isActive()   const { return active_; }
    float intensity()  const { return intensity_; }
    float snowLevel()  const { return snowLevel_; }   // 0-1 world cover
    bool  hasIce()     const { return snowLevel_ > 0.4f; } // lake/river ice
    const SnowVisuals& visuals() const { return vis_; }

private:
    bool        active_    = false;
    float       intensity_ = 0.0f;
    float       snowLevel_ = 0.0f;
    SnowVisuals vis_;
};

} // namespace WM
