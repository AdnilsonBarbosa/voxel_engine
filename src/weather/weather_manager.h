#pragma once
// WeatherManager — master orchestrator of the Atmospheric Weather System.
//
// Coordinates:
//   WeatherController  — picks next type and builds WeatherState
//   WeatherTransition  — ensures meteorologically plausible state hops
//   WindManager        — tracks wind speed, direction, gusts
//   FogManager         — probability-based environmental fog
//   RainManager        — rain particle control
//   SnowManager        — snow particle control + world accumulation
//   StormManager       — storm lifecycle (building/peak/weakening)
//   LightningManager   — flash + thunder timing (included separately)
//
// Usage (main.cpp):
//   weather.load("world_weather.dat");
//   weather.update(dt, totalGameSecs, calendar, biome, hourOfDay, playerY, seaLevel);
//
#include "weather_defs.h"
#include "weather_events.h"
#include "weather_controller.h"
#include "weather_transition.h"
#include "wind_manager.h"
#include "fog_manager.h"
#include "rain_manager.h"
#include "snow_manager.h"
#include "storm_manager.h"
#include "climate_region.h"
#include "time_defs.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include "SDL.h"

namespace WM {

// Transition lerp duration (real seconds). WeatherTransition may override.
static constexpr float BASE_TRANSITION_SECS = 45.0f;

class WeatherManager {
public:
    WeatherManager() {
        rng_ = 12345u;
        applyNewWeather(WeatherType::Clear, 0.1f, 0.0, 15.0f, 50.0f);
        from_ = to_; displayed_ = to_;
        displayed_.transition = 1.0f;
        expiresAt_ = -1.0; // sentinel: set on first update()
        transitionDur_ = BASE_TRANSITION_SECS;
    }

    // ── Frame update ─────────────────────────────────────────────────────────
    void update(float dt, double totalGameSecs, const WT::CalendarState& cal,
                uint8_t biome,
                float hourOfDay = 12.0f, // 0-24 (for FogManager)
                float playerY   = 64.0f, // player altitude
                float seaLevel  = 64.0f, // terrain base height
                float seasonWindMult = 1.0f) // SeasonManager wind scaling
    {
        // ── 1. Smooth lerp every frame ────────────────────────────────────
        transAccum_ = fminf(transAccum_ + dt, transitionDur_);
        float t = transAccum_ / transitionDur_;
        t = t * t * (3.0f - 2.0f * t); // smooth step
        lerpDisplayed_(t);

        // ── 2. Expiry check (game-time-driven) ────────────────────────────
        if (expiresAt_ < 0.0) {
            // First frame: anchor expiry relative to current game time
            expiresAt_ = totalGameSecs + ctrl_.pickDurationSecs(currentType_, rng_);
            SDL_Log("[Weather] Init: %s, expires in %.1fh",
                    weatherName(currentType_),
                    (expiresAt_ - totalGameSecs) / 3600.0);
        }

        if (totalGameSecs >= expiresAt_) {
            const ClimateRegion& climate = climateFor(biome);
            float tempC   = celsiusFor(climate.temperature, cal.season);
            float humidPc = humidityFor(climate.humidity, currentType_);

            WeatherType raw  = ctrl_.pickNext(currentType_, climate, cal.season, rng_);
#ifdef __ANDROID__
            // Android test cycle must visibly change state instead of drawing
            // the same weather again after an expiry.
            if (raw == currentType_) {
                const int order[] = { 0, 2, 4, 5, 6, 7, 8, 9, 10 };
                int at = 0;
                for (int i = 0; i < 9; ++i) if (order[i] == (int)currentType_) { at = i; break; }
                raw = (WeatherType)order[(at + 1) % 9];
            }
#endif
            WeatherType next = transition_.resolve(currentType_, raw); // plausibility check
            float       intn = ctrl_.pickIntensity(next, rng_);
            double      dur  = ctrl_.pickDurationSecs(next, rng_);

            expiresAt_     = totalGameSecs + dur;
            transitionDur_ = transition_.transitionDur(currentType_, next);

            from_      = displayed_;
            from_.transition = 0.0f;
            transAccum_ = 0.0f;

            WeatherType prev = currentType_;
            applyNewWeather(next, intn, totalGameSecs, tempC, humidPc);
            fireEvents_(prev, next);
        }

        // ── 3. Sub-manager updates ────────────────────────────────────────
        wind_.update(dt,
                     displayed_.windSpeed * seasonWindMult,
                     displayed_.windDir,
                     displayed_.type == WeatherType::Thunderstorm ||
                     displayed_.type == WeatherType::Blizzard);

        fog_.update(dt, displayed_, biome, hourOfDay, playerY, seaLevel);

        rain_.update(dt, displayed_, wind_);
        snow_.update(dt, displayed_, wind_);
        storm_.update(dt, displayed_);

        // ── 4. Slow path: temp/humidity at 0.1 Hz ────────────────────────
        slowAccum_ += dt;
        if (slowAccum_ >= 10.0f) {
            slowAccum_ = 0.0f;
            const ClimateRegion& climate = climateFor(biome);
            displayed_.temperature = celsiusFor(climate.temperature, cal.season);
            displayed_.humidity    = humidityFor(climate.humidity, currentType_);
        }

        // ── 5. Auto-save every 60 real seconds ────────────────────────────
        saveAccum_ += dt;
        if (saveAccum_ >= 60.0f && savePath_[0]) {
            saveAccum_ = 0.0f;
            save(savePath_);
        }
    }

    // ── Public accessors ─────────────────────────────────────────────────────
    const WeatherState& state()    const { return displayed_; }
    float lightMult()              const { return displayed_.lightMult; }
    float cloudCover()             const { return displayed_.cloudCover; }
    float rainRate()               const { return displayed_.rainRate; }
    float snowRate()               const { return displayed_.snowRate; }
    float fogDensity()             const { return displayed_.fogDensity; }
    bool  hasThunder()             const { return displayed_.hasThunder; }
    float visibility()             const { return displayed_.visibility; }
    // Sub-manager read access
    const WindManager&  wind()     const { return wind_; }
    const FogManager&   fog()      const { return fog_; }
    const RainManager&  rain()     const { return rain_; }
    const SnowManager&  snow()     const { return snow_; }
    const StormManager& storm()    const { return storm_; }

    // ── Force weather (debug/testing) ────────────────────────────────────────
    void forceWeather(WeatherType t, float intensity = -1.0f, double gameNow = 0.0) {
        const auto& p = WEATHER_PROFILES[(int)t];
        float inten = (intensity >= 0.0f) ? intensity
                    : (p.minIntensity + p.maxIntensity) * 0.5f;
        double dur   = ctrl_.pickDurationSecs(t, rng_);
        expiresAt_   = gameNow + dur;

        WeatherType prev = currentType_;
        applyNewWeather(t, inten, gameNow, displayed_.temperature, displayed_.humidity);

        // Immediate snap — no transition for debug forcing
        from_       = to_;
        transAccum_ = BASE_TRANSITION_SECS;
        displayed_  = to_;
        displayed_.transition = 1.0f;
        transitionDur_ = BASE_TRANSITION_SECS;

        fireEvents_(prev, t);
        SDL_Log("[Weather] FORCED → %s (%.0f%%) for %.1fh",
                weatherName(t), inten * 100.0f, dur / 3600.0);
    }

    // ── Events ───────────────────────────────────────────────────────────────
    WeatherEvents&       events()       { return events_; }
    const WeatherEvents& events() const { return events_; }

    // ── Debug ─────────────────────────────────────────────────────────────────
    void debugLog() const {
        const WeatherState& s = displayed_;
        SDL_Log("[Weather] %s (%.0f%%) | %.1f°C Hum=%.0f%% Wind=%.0f%%(%.0f°)"
                " | Cloud=%.0f%% Rain=%.0f%% Snow=%.0f%% Fog=%.0f%% Light=%.2f"
                " | Trans=%.0f%% Vis=%.0fm",
                weatherName(s.type), s.intensity * 100.0f,
                s.temperature, s.humidity,
                wind_.speed() * 100.0f, wind_.direction() * 57.296f,
                s.cloudCover * 100.0f, s.rainRate * 100.0f,
                s.snowRate * 100.0f, fog_.density() * 100.0f,
                s.lightMult, s.transition * 100.0f, fog_.fogEnd());
    }

    // ── Save / Load ───────────────────────────────────────────────────────────
    bool load(const char* path) {
        strncpy(savePath_, path, sizeof(savePath_) - 1);
        savePath_[sizeof(savePath_) - 1] = 0;

        FILE* f = fopen(path, "rb");
        if (!f) return false;

        struct Hdr {
            char     magic[4];
            uint32_t version;  // 1 = old 9-type; 2 = new 11-type
            uint8_t  type;
            float    intensity;
            double   expiresAt;
            double   lastChanged;
            uint32_t rng;
        };
        Hdr h;
        bool ok = (fread(&h, sizeof(h), 1, f) == 1)
               && (h.magic[0]=='W' && h.magic[1]=='E' && h.magic[2]=='T' && h.magic[3]=='H')
               && (h.version == 1u || h.version == 2u);
        fclose(f);

        if (ok) {
            rng_           = h.rng;
            expiresAt_     = h.expiresAt;
            lastChangedAt_ = h.lastChanged;

            WeatherType t;
            if (h.version == 1u) {
                t = migrateV1Type(h.type); // migrate old 9-type save
                SDL_Log("[Weather] Migrated v1 save: type %d → %s", h.type, weatherName(t));
            } else {
                t = (h.type < WEATHER_COUNT) ? (WeatherType)h.type : WeatherType::Clear;
            }
            applyNewWeather(t, h.intensity, h.lastChanged, 15.0f, 50.0f);
            from_ = to_; displayed_ = to_;
            displayed_.transition = 1.0f;
            SDL_Log("[Weather] Loaded: %s (%.0f%%)", weatherName(t), h.intensity * 100.0f);
#ifdef __ANDROID__
            // Re-anchor the saved weather with the short Android test cycle.
            expiresAt_ = -1.0;
#endif
        }
        return ok;
    }

    bool save(const char* path) const {
        FILE* f = fopen(path, "wb");
        if (!f) return false;
        struct Hdr {
            char     magic[4];
            uint32_t version;
            uint8_t  type;
            float    intensity;
            double   expiresAt;
            double   lastChanged;
            uint32_t rng;
        };
        Hdr h = {{'W','E','T','H'}, 2u,
                  (uint8_t)currentType_, currentIntensity_,
                  expiresAt_, lastChangedAt_, rng_};
        bool ok = (fwrite(&h, sizeof(h), 1, f) == 1);
        fclose(f);
        return ok;
    }

    bool saveToLoadPath() const {
        return savePath_[0] ? save(savePath_) : false;
    }

private:
    WeatherController ctrl_;
    WeatherTransition transition_;
    WeatherEvents     events_;
    WindManager       wind_;
    FogManager        fog_;
    RainManager       rain_;
    SnowManager       snow_;
    StormManager      storm_;

    WeatherType currentType_      = WeatherType::Clear;
    float       currentIntensity_ = 0.1f;
    double      expiresAt_        = -1.0;
    double      lastChangedAt_    = 0.0;
    uint32_t    rng_              = 12345u;

    WeatherState from_, to_, displayed_;
    float        transAccum_    = BASE_TRANSITION_SECS;
    float        transitionDur_ = BASE_TRANSITION_SECS;
    float        slowAccum_     = 0.0f;
    float        saveAccum_     = 0.0f;
    char         savePath_[256] = {};

    void applyNewWeather(WeatherType t, float intensity, double gameNow,
                         float tempC, float humidPct)
    {
        currentType_      = t;
        currentIntensity_ = intensity;
        lastChangedAt_    = gameNow;
        to_ = ctrl_.makeTargetState(t, intensity, tempC, humidPct);
    }

    void lerpDisplayed_(float t) {
        auto lerp = [](float a, float b, float tt) { return a + (b - a) * tt; };
        displayed_.type       = to_.type;
        displayed_.intensity  = lerp(from_.intensity,  to_.intensity,  t);
        displayed_.cloudCover = lerp(from_.cloudCover, to_.cloudCover, t);
        displayed_.rainRate   = lerp(from_.rainRate,   to_.rainRate,   t);
        displayed_.snowRate   = lerp(from_.snowRate,   to_.snowRate,   t);
        displayed_.fogDensity = lerp(from_.fogDensity, to_.fogDensity, t);
        displayed_.windSpeed  = lerp(from_.windSpeed,  to_.windSpeed,  t);
        displayed_.lightMult  = lerp(from_.lightMult,  to_.lightMult,  t);
        displayed_.cloudR     = lerp(from_.cloudR,     to_.cloudR,     t);
        displayed_.cloudG     = lerp(from_.cloudG,     to_.cloudG,     t);
        displayed_.cloudB     = lerp(from_.cloudB,     to_.cloudB,     t);
        displayed_.visibility = lerp(from_.visibility, to_.visibility, t);
        displayed_.hasThunder = to_.hasThunder && (t > 0.9f);
        displayed_.transition = t;
    }

    void fireEvents_(WeatherType prev, WeatherType next) {
        const WeatherState& s = displayed_;
        events_.fire(WeatherEvent::OnWeatherChanged, s);

        bool prevRain = (prev >= WeatherType::LightRain && prev <= WeatherType::Thunderstorm);
        bool nextRain = (next >= WeatherType::LightRain && next <= WeatherType::Thunderstorm);
        bool prevSnow = (prev == WeatherType::Snow || prev == WeatherType::Blizzard);
        bool nextSnow = (next == WeatherType::Snow || next == WeatherType::Blizzard);

        if (!prevRain && nextRain) events_.fire(WeatherEvent::OnRainStart,  s);
        if ( prevRain && !nextRain) events_.fire(WeatherEvent::OnRainEnd,   s);

        if (next == WeatherType::Thunderstorm)
            events_.fire(WeatherEvent::OnStormStart, s);
        if (!prevSnow && nextSnow)
            events_.fire(WeatherEvent::OnSnowStart,  s);
        if (next == WeatherType::LightFog || next == WeatherType::DenseFog)
            events_.fire(WeatherEvent::OnFogRoll,    s);
        if (next == WeatherType::Clear || next == WeatherType::PartlyCloudy)
            events_.fire(WeatherEvent::OnClearSky,   s);
    }
};

} // namespace WM
