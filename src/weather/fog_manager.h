#pragma once
// FogManager — context-aware probabilistic fog.
// Fog does NOT appear always. Probability depends on time of day, humidity,
// temperature, biome, altitude, and current weather.
//
// Usage:
//   fog.update(dt, ws, biome, hourOfDay, playerY, seaLevel);
//   renderer.setFog(fog.fogStart(), fog.fogEnd());
//
#include "weather_defs.h"
#include <cmath>

namespace WM {

class FogManager {
public:
    // dt          = frame delta (seconds)
    // ws          = current interpolated WeatherState
    // biome       = biome index (0-6, matches climate_region.h)
    // hourOfDay   = 0.0-24.0
    // playerY     = player altitude (world units)
    // seaLevel    = terrain base height (world units, typically ~64)
    void update(float dt, const WeatherState& ws, uint8_t biome,
                float hourOfDay, float playerY, float seaLevel)
    {
        // Throttle probability calculation to 4 Hz — visibility rarely needs per-frame accuracy
        probAccum_ += dt;
        if (probAccum_ >= 0.25f) {
            probAccum_ = 0.0f;
            targetDensity_ = computeTarget_(ws, biome, hourOfDay, playerY, seaLevel);
        }

        // Smooth density transition — fog rolls in/out slowly
        float speed = (targetDensity_ > density_) ? 0.15f : 0.4f; // rolls in slower than it lifts
        density_ += (targetDensity_ - density_) * (1.0f - expf(-dt * speed));
    }

    float density()  const { return density_; }
    bool  isActive() const { return density_ > 0.04f; }

    // Fog render distances — tighter fog when density is high
    float fogStart() const {
        return 8.0f  + (1.0f - density_) * 72.0f;  // 8m dense → 80m clear
    }
    float fogEnd() const {
        return 24.0f + (1.0f - density_) * 176.0f; // 24m dense → 200m clear
    }

    // How much the sky horizon is obscured (0-1) — used to tint horizon in sky shader
    float horizonBlur() const { return fminf(1.0f, density_ * 2.0f); }

private:
    float density_       = 0.0f;
    float targetDensity_ = 0.0f;
    float probAccum_     = 0.0f;

    // Base fog affinity per biome (0=no fog, 1=maximum)
    static constexpr float BIOME_FOG[7] = {
        0.55f, // 0 Ocean        — sea mist common
        0.30f, // 1 Beach        — some coastal mist
        0.25f, // 2 Plains       — open field, less fog
        0.75f, // 3 Forest       — morning mist, high humidity
        0.00f, // 4 Desert       — hot dry air, almost never
        0.50f, // 5 Snowy        — ground-level ice fog
        0.65f, // 6 Mountains    — cloud-level, common
    };

    float computeTarget_(const WeatherState& ws, uint8_t biome,
                         float hour, float playerY, float seaLevel) const
    {
        // Weather-driven fog is always present (from WeatherState.fogDensity)
        float base = ws.fogDensity;

        // Environmental fog probability
        float biomeFactor = (biome < 7) ? BIOME_FOG[biome] : 0.25f;

        // Time-of-day: dawn (5-8h) and dusk (17-20h) peak times for fog
        float timeFactor = 0.0f;
        if (hour >= 4.5f && hour <= 9.0f) {
            timeFactor = 1.0f - fabsf(hour - 6.5f) / 2.5f;
        } else if (hour >= 16.5f && hour <= 21.0f) {
            timeFactor = 1.0f - fabsf(hour - 18.5f) / 2.5f;
        } else if (hour < 4.5f || hour > 21.0f) {
            timeFactor = 0.15f; // slight overnight haze
        }
        timeFactor = fmaxf(0.0f, fminf(1.0f, timeFactor));

        // Humidity: higher humidity → more fog
        float humidFactor = ws.humidity / 100.0f;

        // Temperature: fog most likely in cool air (5-15°C), rare in heat/cold extremes
        float tempRange  = fmaxf(0.0f, 1.0f - fabsf(ws.temperature - 10.0f) / 25.0f);

        // Altitude: fog hugs valleys and sea level; above terrain = less likely
        float altAboveSea  = fmaxf(0.0f, playerY - seaLevel);
        float altFactor    = fmaxf(0.0f, 1.0f - altAboveSea / 35.0f);

        // Environmental fog probability combined
        float envProb = biomeFactor * timeFactor * humidFactor * tempRange * altFactor;
        float envFog  = envProb * 0.50f; // env tops out at 0.50 density

        return fminf(1.0f, base + envFog);
    }
};

} // namespace WM
