#pragma once
// WeatherEvents — event bus for the Dynamic Weather System.
// Other systems (NPCs, agriculture, animals, economy) subscribe once at init
// and receive callbacks whenever the weather changes.
#include "weather_defs.h"
#include <functional>
#include <vector>

namespace WM {

enum class WeatherEvent : uint8_t {
    OnWeatherChanged = 0,  // any transition (always fires)
    OnRainStart      = 1,  // Rain | Storm | Thunderstorm begins
    OnRainEnd        = 2,  // rain-type weather ends
    OnStormStart     = 3,  // Storm | Thunderstorm begins
    OnSnowStart      = 4,  // Snow begins
    OnDrought        = 5,  // Drought begins
    OnFogRoll        = 6,  // Fog begins
    OnThunder        = 7,  // Thunderstorm tick (fire periodically while active)
    OnClearSky       = 8,  // Clear or Sunny begins
    COUNT            = 9
};

// Callback: receives the event type AND the full state at the moment of fire.
using WeatherCallback = std::function<void(WeatherEvent, const WeatherState&)>;

class WeatherEvents {
public:
    void subscribe(WeatherEvent ev, WeatherCallback cb) {
        listeners_[(int)ev].push_back(std::move(cb));
    }

    void fire(WeatherEvent ev, const WeatherState& s) const {
        for (auto& cb : listeners_[(int)ev]) cb(ev, s);
    }

    void clear() {
        for (auto& v : listeners_) v.clear();
    }

    int totalSubscribers() const {
        int n = 0;
        for (auto& v : listeners_) n += (int)v.size();
        return n;
    }

private:
    std::vector<WeatherCallback> listeners_[(int)WeatherEvent::COUNT];
};

} // namespace WM
