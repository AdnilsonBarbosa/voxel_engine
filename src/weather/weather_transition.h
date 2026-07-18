#pragma once
// WeatherTransition — enforces meteorologically plausible state progressions.
// Prevents impossible direct jumps (Clear→Thunderstorm, Blizzard→Sunny).
// The controller picks the best next state; this class validates and may insert
// an intermediary step so transitions feel natural.
#include "weather_defs.h"

namespace WM {

class WeatherTransition {
public:
    // Validate/redirect the desired next transition.
    // Returns the type we should ACTUALLY transition to next.
    // If an intermediary is needed, returns that instead of `desired`.
    WeatherType resolve(WeatherType current, WeatherType desired) const {
        int c = (int)current;
        int d = (int)desired;

        // Precipitation group: LightRain, HeavyRain, Thunderstorm
        bool cPrecip = (c >= 4 && c <= 6);
        bool dPrecip = (d >= 4 && d <= 6);

        // Snow group: Snow, Blizzard
        bool cSnow = (c >= 9);
        bool dSnow = (d >= 9);

        // Rule 1: Can't jump from Clear/PartlyCloudy directly to rain/thunder
        if (c <= 1 && dPrecip)
            return WeatherType::Cloudy;

        // Rule 2: Can't jump from Clear/PartlyCloudy directly to Overcast
        if (c == 0 && d == (int)WeatherType::Overcast)
            return WeatherType::PartlyCloudy;

        // Rule 3: Thunderstorm must wind down through HeavyRain before clear
        if (c == (int)WeatherType::Thunderstorm && !dPrecip && !dSnow)
            return WeatherType::HeavyRain;

        // Rule 4: HeavyRain must step down through LightRain before clear
        if (c == (int)WeatherType::HeavyRain && d <= (int)WeatherType::Cloudy)
            return WeatherType::LightRain;

        // Rule 5: Blizzard winds down through Snow
        if (c == (int)WeatherType::Blizzard && !dSnow)
            return WeatherType::Snow;

        // Rule 6: Snow can't jump directly to Clear (go through Cloudy)
        if (c == (int)WeatherType::Snow && d <= (int)WeatherType::PartlyCloudy)
            return WeatherType::Cloudy;

        // Rule 7: DenseFog should step through LightFog before clear sky
        if (c == (int)WeatherType::DenseFog && d <= (int)WeatherType::Cloudy)
            return WeatherType::LightFog;

        return desired; // transition is already plausible
    }

    // Duration of the TRANSITION itself (real seconds, lerp duration)
    // Slower for extreme transitions (storm→clear), faster for mild ones.
    float transitionDur(WeatherType from, WeatherType to) const {
        int f = (int)from, t = (int)to;
        // Extremes: Thunderstorm or Blizzard transitions take longer
        if (f >= 6 || t >= 6) return 45.0f;
        // Fog building/lifting is slow
        if (f >= 7 || t >= 7) return 30.0f;
        // Normal
        return 20.0f;
    }
};

} // namespace WM
