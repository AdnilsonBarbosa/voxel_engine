#pragma once
// Atmospheric Weather System — types, profiles, and runtime state.
// No engine dependencies — safe to include from any header.
#include <cstdint>
#include <cmath>

namespace WM {

// ── Weather states ─────────────────────────────────────────────────────────────
enum class WeatherType : uint8_t {
    Clear        = 0,   // full sun, minimal clouds
    PartlyCloudy = 1,   // 20-40% cloud coverage
    Cloudy       = 2,   // 40-70% coverage, still bright
    Overcast     = 3,   // 70-100% coverage, grey sky
    LightRain    = 4,   // gentle rain, 30-50% rate
    HeavyRain    = 5,   // strong rain, 60-90% rate
    Thunderstorm = 6,   // heavy rain + lightning
    LightFog     = 7,   // shallow mist, 20-40% density
    DenseFog     = 8,   // thick fog, 50-90% density
    Snow         = 9,   // moderate snowfall
    Blizzard     = 10,  // heavy snow + strong wind
    COUNT        = 11
};
static constexpr int WEATHER_COUNT = (int)WeatherType::COUNT;

// Static compile-time profile for each weather state.
struct WeatherProfile {
    const char* name;
    float minIntensity, maxIntensity;
    float minDurHours,  maxDurHours;  // game hours
    float cloudCover;   // 0-1 target sky coverage
    float rainRate;     // 0-1 precipitation rate
    float snowRate;     // 0-1
    float fogDensity;   // 0-1
    float lightMult;    // multiplier on sunlight (storms darken)
    float windMult;     // 0-1 base wind strength
    bool  thunder;
    // Sky atmosphere colours for this state
    float cloudR, cloudG, cloudB; // cloud colour (normalised)
};

// clang-format off
static constexpr WeatherProfile WEATHER_PROFILES[WEATHER_COUNT] = {
// name             minI  maxI  minH  maxH  cloud rain  snow  fog   light wind  thnd  cldR  cldG  cldB
{"Clear",           0.0f, 0.2f, 2.0f, 8.0f, 0.05f,0.00f,0.00f,0.00f,1.00f,0.05f,false,1.00f,1.00f,1.00f},
{"PartlyCloudy",    0.2f, 0.5f, 1.5f, 5.0f, 0.30f,0.00f,0.00f,0.00f,0.95f,0.10f,false,0.97f,0.97f,1.00f},
{"Cloudy",          0.4f, 0.7f, 1.0f, 4.0f, 0.65f,0.00f,0.00f,0.05f,0.85f,0.18f,false,0.88f,0.89f,0.92f},
{"Overcast",        0.6f, 0.9f, 0.5f, 3.0f, 0.90f,0.00f,0.00f,0.10f,0.70f,0.22f,false,0.65f,0.67f,0.70f},
{"LightRain",       0.3f, 0.7f, 0.5f, 3.0f, 0.80f,0.45f,0.00f,0.15f,0.75f,0.30f,false,0.63f,0.65f,0.70f},
{"HeavyRain",       0.6f, 1.0f, 0.3f, 2.0f, 0.95f,0.85f,0.00f,0.20f,0.55f,0.50f,false,0.48f,0.50f,0.56f},
{"Thunderstorm",    0.8f, 1.0f, 0.2f, 1.5f, 1.00f,1.00f,0.00f,0.00f,0.40f,0.70f,true, 0.28f,0.28f,0.33f},
{"LightFog",        0.3f, 0.6f, 0.5f, 3.0f, 0.40f,0.05f,0.00f,0.35f,0.82f,0.05f,false,0.84f,0.86f,0.90f},
{"DenseFog",        0.6f, 1.0f, 0.3f, 2.0f, 0.60f,0.05f,0.00f,0.75f,0.65f,0.05f,false,0.74f,0.76f,0.80f},
{"Snow",            0.3f, 0.8f, 1.0f, 6.0f, 0.75f,0.00f,0.65f,0.15f,0.82f,0.22f,false,0.90f,0.92f,0.96f},
{"Blizzard",        0.8f, 1.0f, 0.3f, 2.5f, 0.95f,0.00f,1.00f,0.35f,0.55f,0.90f,false,0.72f,0.73f,0.76f},
};
// clang-format on

// ── Runtime snapshot (smooth-interpolated every frame) ────────────────────────
struct WeatherState {
    WeatherType type        = WeatherType::Clear;
    float       intensity   = 0.0f;
    float       cloudCover  = 0.05f;
    float       rainRate    = 0.0f;
    float       snowRate    = 0.0f;
    float       fogDensity  = 0.0f;
    float       windSpeed   = 0.05f;  // 0-1
    float       windDir     = 0.0f;   // radians
    float       temperature = 15.0f;  // °C display
    float       humidity    = 50.0f;  // 0-100% display
    float       lightMult   = 1.0f;
    bool        hasThunder  = false;
    float       transition  = 1.0f;   // 0=just changed, 1=settled
    float       visibility  = 200.0f; // effective fog end distance (world units)
    // Sky/cloud colours (interpolated, read by Sky::render)
    float       cloudR = 1.0f, cloudG = 1.0f, cloudB = 1.0f;
};

inline const char* weatherName(WeatherType t) {
    int i = (int)t;
    return (i >= 0 && i < WEATHER_COUNT) ? WEATHER_PROFILES[i].name : "Unknown";
}

// Map v1 save (9-type) type byte to current (11-type) WeatherType.
inline WeatherType migrateV1Type(uint8_t v1) {
    // v1: 0=Clear 1=Sunny 2=Cloudy 3=Rain 4=Storm 5=Thunderstorm 6=Snow 7=Fog 8=Drought
    static const WeatherType TABLE[9] = {
        WeatherType::Clear,        // 0 Clear
        WeatherType::Clear,        // 1 Sunny → Clear
        WeatherType::Cloudy,       // 2 Cloudy
        WeatherType::LightRain,    // 3 Rain → LightRain
        WeatherType::HeavyRain,    // 4 Storm → HeavyRain
        WeatherType::Thunderstorm, // 5 Thunderstorm
        WeatherType::Snow,         // 6 Snow
        WeatherType::LightFog,     // 7 Fog → LightFog
        WeatherType::Clear,        // 8 Drought → Clear
    };
    return (v1 < 9) ? TABLE[v1] : WeatherType::Clear;
}

} // namespace WM
