#pragma once
// RainManager — drives rain particles with intensity-aware spawn rates,
// wind-adjusted angles, and visual adaptation per rain type.
//
// Usage:
//   rain.update(dt, ws, wind, camPos, camFwd);
//   rain.draw(particles, mvp, time);
//
#include "weather_defs.h"
#include "wind_manager.h"

namespace WM {

struct RainVisuals {
    float spawnRate   = 1.0f;   // multiplier on base spawn speed
    float dropSize    = 2.5f;   // base particle size
    float dropStretch = 7.5f;   // horizontal compression
    float fallSpeed   = 18.0f;  // m/s downward
    float alpha       = 0.85f;  // particle alpha
    float windAngle   = 0.0f;   // lateral push m/s (from wind)
};

class RainManager {
public:
    void update(float dt, const WeatherState& ws, const WindManager& wind) {
        float rainRate = ws.rainRate;
        if (rainRate < 0.01f) {
            active_ = false;
            intensity_ = 0.0f;
            return;
        }
        active_    = true;
        intensity_ = rainRate * ws.intensity;

        // Adapt visuals to intensity
        bool heavy = (ws.type == WeatherType::HeavyRain ||
                      ws.type == WeatherType::Thunderstorm);

        vis_.spawnRate   = heavy ? 3.5f : 2.2f;
        vis_.dropSize    = heavy ? 2.8f : 2.0f;
        vis_.dropStretch = heavy ? 7.0f : 8.5f; // heavier rain = slightly less stretch
        vis_.fallSpeed   = heavy ? 22.0f : 16.0f;
        vis_.alpha       = heavy ? 0.88f : 0.80f;
        vis_.windAngle   = wind.particleX(); // lateral push from wind
    }

    bool  isActive()    const { return active_; }
    float intensity()   const { return intensity_; }
    const RainVisuals& visuals() const { return vis_; }

private:
    bool        active_    = false;
    float       intensity_ = 0.0f;
    RainVisuals vis_;
};

} // namespace WM
