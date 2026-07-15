#pragma once
// RainSound — prepared interface for rain audio.
// No actual audio playback yet (no SDL_mixer dependency).
// Provides volume/pitch parameters that a future audio system will consume.
#include "weather_defs.h"
#include "weather_config.h"
#include <cmath>

namespace WM {

struct RainSoundState {
    bool  active     = false;   // is rain audio playing?
    float volume     = 0.0f;    // 0.0=silent, 1.0=full
    float pitch      = 1.0f;    // 1.0=normal, >1=faster rain sound
    float windVolume = 0.0f;    // separate wind channel volume
    float thunderVol = 0.0f;    // thunder crack volume (0 when no thunder)
};

class RainSound {
public:
    // Call every frame (or every N frames for mobile optimization).
    // Updates the sound parameters based on current weather state.
    void update(const WeatherState& state, const WeatherConfig& cfg) {
        bool wasActive = current_.active;

        current_.active = (state.rainRate > 0.01f || state.snowRate > 0.01f);

        // Volume scales with rain intensity
        float rate = fmaxf(state.rainRate, state.snowRate);
        current_.volume = rate * state.intensity;

        // Pitch slightly higher for heavier rain (denser drops = higher frequency)
        current_.pitch = 1.0f + rate * 0.15f;

        // Wind volume follows wind speed
        current_.windVolume = state.windSpeed * 0.6f;

        // Thunder volume: 0 when no thunder, pulses when hasThunder
        current_.thunderVol = state.hasThunder ? 0.8f * state.intensity : 0.0f;

        // Fire events for the audio system
        if (!wasActive && current_.active) {
            onRainAudioStart_();
        } else if (wasActive && !current_.active) {
            onRainAudioStop_();
        }
    }

    const RainSoundState& state() const { return current_; }

    // ── Future audio system hooks ────────────────────────────────────────────
    // When SDL_mixer is added, these will trigger sound channels:
    //   onRainAudioStart_() → play looped rain_ambient.wav at volume
    //   onRainAudioStop_()  → fade out over 2 seconds
    //   thunderVol > 0      → trigger one-shot thunder_XX.wav

private:
    RainSoundState current_;

    void onRainAudioStart_() {
        // Future: SDL_PlayChannel(RAIN_CHANNEL, rain_loop, -1);
        // SDL_MixVolume(RAIN_CHANNEL, (int)(current_.volume * 128));
    }
    void onRainAudioStop_() {
        // Future: SDL_FadeOutChannel(RAIN_CHANNEL, 2000);
    }
};

} // namespace WM
