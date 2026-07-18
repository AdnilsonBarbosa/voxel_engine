#pragma once
// WeatherController — stateless helpers for weather selection and state computation.
// Uses a 11×11 transition probability matrix + biome/season/temperature modifiers.
#include "weather_defs.h"
#include "climate_region.h"

namespace WM {

inline uint32_t lcg(uint32_t& s)  { return (s = s * 1664525u + 1013904223u); }
inline float    lcgf(uint32_t& s) { return (float)(lcg(s) >> 8) / (float)(1 << 24); }

// ── Transition matrix [from][to] ──────────────────────────────────────────────
// Higher value = more likely when coming FROM that state.
// Encodes meteorological realism: gradual escalation and de-escalation.
//                                  Clr  PCl  Cld  Ovr  LRn  HRn  Tnd  LFg  DFg  Snw  Blz
static const float TRANSITION[WEATHER_COUNT][WEATHER_COUNT] = {
/* Clear        */  {2.5f,3.0f,1.0f,0.0f,0.0f,0.0f,0.0f,0.5f,0.0f,0.5f,0.0f},
/* PartlyCloudy */  {2.0f,2.0f,3.0f,0.5f,0.0f,0.0f,0.0f,1.0f,0.0f,0.3f,0.0f},
/* Cloudy       */  {1.0f,2.0f,1.5f,3.0f,2.5f,0.5f,0.0f,1.5f,0.5f,1.0f,0.0f},
/* Overcast     */  {0.3f,0.8f,2.0f,1.5f,3.0f,2.5f,0.5f,1.0f,1.0f,2.0f,0.5f},
/* LightRain    */  {0.3f,0.3f,2.0f,2.0f,2.0f,2.5f,1.0f,1.0f,0.5f,0.5f,0.0f},
/* HeavyRain    */  {0.0f,0.0f,1.0f,2.0f,3.0f,1.5f,2.5f,0.5f,0.5f,0.0f,0.0f},
/* Thunderstorm */  {0.0f,0.0f,0.5f,1.5f,3.5f,3.0f,1.0f,0.0f,0.0f,0.0f,0.0f},
/* LightFog     */  {1.5f,1.5f,2.0f,1.5f,1.5f,0.5f,0.0f,1.5f,2.0f,0.5f,0.0f},
/* DenseFog     */  {0.5f,0.5f,1.5f,2.0f,1.5f,1.0f,0.0f,2.5f,1.0f,0.5f,0.0f},
/* Snow         */  {0.3f,0.3f,1.5f,2.0f,0.5f,0.0f,0.0f,1.0f,0.5f,2.5f,2.5f},
/* Blizzard     */  {0.0f,0.0f,0.3f,1.0f,0.0f,0.0f,0.0f,0.5f,0.5f,3.5f,1.5f},
};

inline WeatherType selectWeighted(const float weights[], uint32_t& rng) {
    float total = 0.0f;
    for (int i = 0; i < WEATHER_COUNT; i++) total += weights[i];
    if (total <= 0.0f) return WeatherType::Clear;
    float r = lcgf(rng) * total, sum = 0.0f;
    for (int i = 0; i < WEATHER_COUNT; i++) {
        sum += weights[i];
        if (r <= sum) return (WeatherType)i;
    }
    return WeatherType::Clear;
}

class WeatherController {
public:
    WeatherType pickNext(WeatherType cur, const ClimateRegion& climate,
                         WT::Season season, uint32_t& rng) const
    {
        float w[WEATHER_COUNT];
        const float* tr = TRANSITION[(int)cur];
        for (int i = 0; i < WEATHER_COUNT; i++)
            w[i] = climate.baseWeights[i] * tr[i];
        applySeasonMod(w, season);
        applyTempMod(w, climate.temperature * 30.0f + 10.0f);
        return selectWeighted(w, rng);
    }

    float pickIntensity(WeatherType t, uint32_t& rng) const {
        const auto& p = WEATHER_PROFILES[(int)t];
        return p.minIntensity + lcgf(rng) * (p.maxIntensity - p.minIntensity);
    }

    double pickDurationSecs(WeatherType t, uint32_t& rng) const {
        const auto& p = WEATHER_PROFILES[(int)t];
        float hours = p.minDurHours + lcgf(rng) * (p.maxDurHours - p.minDurHours);
#ifdef __ANDROID__
        // Short cycle on Android builds so weather can be tested quickly.
        const float hold = (t == WeatherType::Snow || t == WeatherType::Blizzard) ? 0.50f : 0.35f;
#else
        const float hold = 1.0f;
#endif
        return (double)(hours * hold) * 3600.0;
    }

    // Compute target WeatherState from type + intensity + environment.
    WeatherState makeTargetState(WeatherType t, float intensity,
                                 float tempC, float humidPct) const
    {
        const auto& p = WEATHER_PROFILES[(int)t];
        WeatherState s;
        s.type        = t;
        s.intensity   = intensity;
        s.cloudCover  = fminf(1.0f, p.cloudCover * intensity + 0.02f);
        s.rainRate    = p.rainRate   * intensity;
        s.snowRate    = p.snowRate   * intensity;
        s.fogDensity  = p.fogDensity * intensity;
        s.windSpeed   = p.windMult   * intensity;
        s.windDir     = 0.0f; // WindManager controls actual direction
        s.lightMult   = 1.0f - (1.0f - p.lightMult) * intensity;
        s.hasThunder  = p.thunder && (intensity > 0.75f);
        s.temperature = tempC;
        s.humidity    = humidPct;
        s.transition  = 0.0f;
        // Cloud colour
        s.cloudR = p.cloudR;
        s.cloudG = p.cloudG;
        s.cloudB = p.cloudB;
        // Effective visibility (fog reduces draw distance)
        s.visibility = 200.0f * (1.0f - s.fogDensity * 0.85f);
        return s;
    }
};

} // namespace WM
