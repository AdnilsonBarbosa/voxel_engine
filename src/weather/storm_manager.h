#pragma once
// StormManager — orchestrates storm escalation and de-escalation.
// Tracks storm lifecycle: building → peak → weakening.
// Feeds into LightningManager enable/disable and rain rate scaling.
#include "weather_defs.h"
#include <cmath>

namespace WM {

enum class StormPhase : uint8_t {
    None      = 0, // no storm active
    Building  = 1, // clouds darkening, wind rising
    Peak      = 2, // full intensity, thunder+lightning
    Weakening = 3, // rain tapering off, thunder rare
};

class StormManager {
public:
    void update(float dt, const WeatherState& ws) {
        bool isStorm = (ws.type == WeatherType::Thunderstorm ||
                        ws.type == WeatherType::HeavyRain);

        if (!isStorm) {
            if (phase_ != StormPhase::None) {
                // Storm just ended — fast decay
                stormIntensity_ -= dt * 0.5f;
                if (stormIntensity_ <= 0.0f) {
                    stormIntensity_ = 0.0f;
                    phase_          = StormPhase::None;
                }
            }
            return;
        }

        // Build toward full intensity
        float target = (ws.type == WeatherType::Thunderstorm)
                     ? ws.intensity : ws.intensity * 0.6f;

        if (stormIntensity_ < target) {
            stormIntensity_ += dt * 0.08f;
            phase_ = StormPhase::Building;
        } else {
            stormIntensity_ = target;
            phase_ = StormPhase::Peak;
        }
        stormIntensity_ = fminf(1.0f, stormIntensity_);

        // Transition to weakening phase in final 20% of weather duration
        // (We don't have remaining duration here, so use intensity drop)
        if (ws.transition < 0.2f && phase_ == StormPhase::Peak) {
            phase_ = StormPhase::Weakening;
        }
    }

    StormPhase phase()     const { return phase_; }
    float intensity()      const { return stormIntensity_; }
    bool isActive()        const { return phase_ != StormPhase::None; }
    bool isPeak()          const { return phase_ == StormPhase::Peak; }
    bool lightningEnabled()const { return phase_ == StormPhase::Peak || phase_ == StormPhase::Building; }
    float rainBoost()      const { return 1.0f + stormIntensity_ * 0.3f; } // rain appears heavier in storms

private:
    StormPhase phase_          = StormPhase::None;
    float      stormIntensity_ = 0.0f;
};

} // namespace WM
