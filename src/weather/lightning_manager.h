#pragma once
// LightningManager — handles lightning flashes and thunder timing during storms.
//
// Usage (main.cpp):
//   WM::LightningManager lightning;
//   // per-frame:
//   lightning.update(dt, weather.hasThunder(), weather.state().intensity);
//   float flash = lightning.flashBrightness();  // multiply sun light by this
//   if (lightning.thunderJustFired()) SDL_Log("[Thunder] CRACK!");
//
#include "weather_defs.h"
#include <cmath>
#include "SDL.h"

namespace WM {

class LightningManager {
public:
    // dt             = real-time delta (seconds)
    // hasThunder     = weather.hasThunder()
    // stormIntensity = weather.state().intensity  (0..1)
    void update(float dt, bool hasThunder, float stormIntensity) {
        thunderJustFired_ = false;

        if (!hasThunder) {
            // No storm: the flash multiplier rests at its NEUTRAL value 1.0
            // (it multiplies sun light, so 0.0 here would black out the world).
            // Decay any active flash spike smoothly back down to 1.0.
            flashTimer_  = fmaxf(0.0f, flashTimer_ - dt);
            flashBright_ = fmaxf(1.0f, flashBright_ - dt * 8.0f);
            strikeAccum_ = 0.0f;
            return;
        }

        // ── Thunder / strike timer ────────────────────────────────────────────
        strikeAccum_ += dt;
        if (strikeAccum_ >= nextStrikeInterval_) {
            strikeAccum_ = 0.0f;

            // Randomise next interval based on storm intensity:
            //   light storm → 8-15s between strikes
            //   heavy storm → 2-6s between strikes
            float minInterval = 12.0f - stormIntensity * 10.0f;  // 2–12s
            float maxInterval = 20.0f - stormIntensity * 14.0f;  // 6–20s
            nextStrikeInterval_ = minInterval + lcgf_() * (maxInterval - minInterval);
            nextStrikeInterval_ = fmaxf(1.5f, nextStrikeInterval_);

            // Thunder sound delay: 1s per 340m — lightning hits ~50-400m away
            float dist = 50.0f + lcgf_() * 350.0f;
            thunderDelaySecs_ = dist / 340.0f;
            thunderDelayAccum_ = 0.0f;
            pendingThunder_    = true;

            // Visual flash: bright spike followed by decay
            flashBright_ = 1.5f + stormIntensity * 1.2f;  // 1.5–2.7x brightness
            flashTimer_  = 0.08f + lcgf_() * 0.06f;       // 0.08–0.14s flash

            SDL_Log("[Lightning] Strike! flash=%.2fx  thunder in %.1fs",
                    flashBright_, thunderDelaySecs_);
        }

        // ── Flash decay ───────────────────────────────────────────────────────
        if (flashTimer_ > 0.0f) {
            flashTimer_ -= dt;
            if (flashTimer_ <= 0.0f) {
                flashTimer_  = 0.0f;
                flashBright_ = 1.0f;  // back to normal after flash
            }
        } else if (flashBright_ > 1.0f) {
            // Post-flash: smooth ramp back to 1.0
            flashBright_ = fmaxf(1.0f, flashBright_ - dt * 6.0f);
        }

        // ── Thunder sound delay ───────────────────────────────────────────────
        if (pendingThunder_) {
            thunderDelayAccum_ += dt;
            if (thunderDelayAccum_ >= thunderDelaySecs_) {
                pendingThunder_   = false;
                thunderJustFired_ = true;
                SDL_Log("[Thunder] RUMBLE (%.0fm away)", thunderDelaySecs_ * 340.0f);
            }
        }
    }

    // Brightness multiplier: 1.0 = normal; >1.0 = lightning flash.
    // Apply to sun light: renderer.setLight(sunLight * weather.lightMult() * flashBrightness())
    float flashBrightness() const { return flashBright_; }

    // True for exactly one frame when thunder sound should play.
    bool thunderJustFired() const { return thunderJustFired_; }

    // True while the visual flash is active.
    bool isFlashing() const { return flashTimer_ > 0.0f || flashBright_ > 1.01f; }

    // Distance of the thunder strike (metres). Valid when thunderJustFired().
    float thunderDistanceM() const { return thunderDelaySecs_ * 340.0f; }

private:
    float flashBright_       = 1.0f;
    float flashTimer_        = 0.0f;   // seconds remaining in the flash spike

    float strikeAccum_       = 0.0f;
    float nextStrikeInterval_= 5.0f;   // first strike after 5 real seconds

    bool  pendingThunder_    = false;
    float thunderDelaySecs_  = 0.0f;
    float thunderDelayAccum_ = 0.0f;
    bool  thunderJustFired_  = false;

    // Minimal LCG for determinism without heavy RNG overhead
    uint32_t rng_ = 98765u;
    float lcgf_() {
        rng_ = rng_ * 1664525u + 1013904223u;
        return (float)(rng_ >> 8) / (float)(1 << 24);
    }
};

} // namespace WM
