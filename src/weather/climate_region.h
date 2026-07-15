#pragma once
// ClimateRegion — maps biome to base weather probabilities for the 11-type system.
// Biome indices match Biomes::Biome from biome.h:
//   0=Ocean  1=Beach  2=Plains  3=Forest  4=Desert  5=Snowy  6=Mountains
#include "weather_defs.h"
#include "time_defs.h"

namespace WM {

struct ClimateRegion {
    const char* biomeName;
    float temperature; // -1 frozen … +1 scorching (for display mapping)
    float humidity;    // 0.0 arid … 1.0 saturated
    // Base probability weight for each of the 11 weather types (unnormalised).
    // Combined with the transition matrix in WeatherController.
    float baseWeights[WEATHER_COUNT];
};

// clang-format off
//                                   Clr  PCl  Cld  Ovr  LRn  HRn  Tnd  LFg  DFg  Snw  Blz
static const ClimateRegion BIOME_CLIMATE[7] = {
{"Ocean",      0.1f, 0.9f, { 5,  5,  8,  6,  7,  9,  5,  5,  3,  1,  1 }},
{"Beach",      0.4f, 0.6f, {10,  9,  8,  5,  5,  3,  2,  4,  2,  0,  0 }},
{"Plains",     0.2f, 0.5f, { 8,  8, 10,  6,  7,  5,  3,  4,  2,  3,  1 }},
{"Forest",     0.0f, 0.8f, { 4,  4,  9,  7, 10,  8,  4,  9,  7,  4,  1 }},
{"Desert",     0.9f, 0.1f, { 7, 10,  5,  3,  0,  0,  0,  2,  1,  0,  0 }},
{"Snowy",     -0.8f, 0.6f, { 4,  3,  6,  5,  3,  3,  3,  3,  3, 18,  8 }},
{"Mountains", -0.3f, 0.5f, { 6,  5,  8,  8,  7,  6,  7,  6,  4, 11,  7 }},
};
// clang-format on
static constexpr int BIOME_CLIMATE_COUNT = 7;

inline const ClimateRegion& climateFor(uint8_t biome) {
    return BIOME_CLIMATE[biome < BIOME_CLIMATE_COUNT ? biome : 2]; // default Plains
}

// Display temperature: biome base + season offset.
inline float celsiusFor(float bioTemp, WT::Season season) {
    float base = bioTemp * 30.0f + 10.0f; // -1 → -20°C, 0 → +10°C, +1 → +40°C
    switch (season) {
        case WT::Season::Summer: return base + 10.0f;
        case WT::Season::Winter: return base - 10.0f;
        case WT::Season::Autumn: return base -  3.0f;
        default:                 return base;
    }
}

// Humidity percentage for display: biome base adjusted by current weather.
inline float humidityFor(float bioHumid, WeatherType weather) {
    float base = bioHumid * 100.0f;
    switch (weather) {
        case WeatherType::LightRain:
        case WeatherType::HeavyRain:
        case WeatherType::Thunderstorm: return fminf(100.0f, base + 30.0f);
        case WeatherType::LightFog:
        case WeatherType::DenseFog:     return fminf(100.0f, base + 25.0f);
        case WeatherType::Snow:
        case WeatherType::Blizzard:     return fminf(100.0f, base + 15.0f);
        default:                        return base;
    }
}

// Apply season multipliers to weight array (in-place).
inline void applySeasonMod(float w[], WT::Season s) {
    // clang-format off
    //                          Clr   PCl   Cld   Ovr   LRn   HRn   Tnd   LFg   DFg   Snw   Blz
    static const float M[4][WEATHER_COUNT] = {
    /* Spring */ {1.0f, 1.0f, 1.2f, 1.0f, 1.5f, 1.2f, 1.0f, 1.3f, 0.8f, 0.3f, 0.1f},
    /* Summer */ {0.8f, 1.2f, 0.8f, 0.6f, 0.7f, 0.8f, 0.8f, 0.5f, 0.3f, 0.0f, 0.0f},
    /* Autumn */ {1.0f, 0.9f, 1.3f, 1.4f, 1.2f, 1.0f, 0.8f, 2.0f, 1.5f, 0.6f, 0.3f},
    /* Winter */ {0.8f, 0.6f, 1.0f, 1.2f, 0.6f, 0.5f, 0.4f, 0.8f, 0.7f, 2.5f, 1.8f},
    };
    // clang-format on
    const float* m = M[(int)s];
    for (int i = 0; i < WEATHER_COUNT; i++) w[i] *= m[i];
}

// Apply temperature modifiers: extreme cold blocks drought/sun, extreme heat blocks snow.
inline void applyTempMod(float w[], float temperature) {
    if (temperature < -5.0f) {
        // Very cold: snow and blizzard much more likely
        w[(int)WeatherType::Snow]        *= 3.5f;
        w[(int)WeatherType::Blizzard]    *= 3.0f;
        w[(int)WeatherType::LightRain]   *= 0.3f;
        w[(int)WeatherType::HeavyRain]   *= 0.2f;
        w[(int)WeatherType::Thunderstorm] = 0.0f;
    } else if (temperature > 28.0f) {
        // Very hot: snow impossible, drought conditions
        w[(int)WeatherType::Snow]        = 0.0f;
        w[(int)WeatherType::Blizzard]    = 0.0f;
        w[(int)WeatherType::LightRain]  *= 0.4f;
        w[(int)WeatherType::HeavyRain]  *= 0.3f;
    } else if (temperature > 15.0f) {
        // Warm: slightly less snow, still possible in mountains/snowy biomes
        w[(int)WeatherType::Snow]       *= 0.2f;
        w[(int)WeatherType::Blizzard]   *= 0.1f;
    }
}

} // namespace WM
