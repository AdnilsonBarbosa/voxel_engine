#pragma once
// WindManager — tracks wind speed, direction, and gusts.
// Influences cloud drift, rain angle, snow particle movement.
// Zero heap allocation, no heavy math — fits in the 60-FPS hot path.
#include <cmath>
#include <cstdint>

namespace WM {

class WindManager {
public:
    // dt            = real-time delta (seconds)
    // targetSpeed   = 0-1 desired base wind strength (from WeatherProfile.windMult * intensity)
    // targetDir     = desired wind direction in radians (slowly varies)
    // stormy        = true for Thunderstorm/Blizzard (gusts more frequent + stronger)
    void update(float dt, float targetSpeed, float targetDir, bool stormy) {
        // Smooth transition to target speed and direction
        float speedK = stormy ? 2.5f : 0.8f;
        speed_ += (targetSpeed - speed_) * (1.0f - expf(-dt * speedK));
        // Direction lerp (shortest arc, avoid spinning)
        float diff = targetDir - dir_;
        while (diff >  PI) diff -= PI * 2.0f;
        while (diff < -PI) diff += PI * 2.0f;
        dir_ += diff * (1.0f - expf(-dt * 0.25f));

        // Gusts: short-lived speed spikes
        gustTimer_ -= dt;
        if (gustTimer_ <= 0.0f) {
            gust_      = (stormy ? 0.25f : 0.08f) * lcgf();
            gustTimer_ = 1.5f + lcgf() * (stormy ? 2.0f : 5.0f);
        }
        gust_ -= dt * (stormy ? 1.0f : 0.4f); // decay
        if (gust_ < 0.0f) gust_ = 0.0f;

        // Slowly drift wind direction over time
        dirDriftTimer_ -= dt;
        if (dirDriftTimer_ <= 0.0f) {
            dirTarget_     = lcgf() * PI * 2.0f;
            dirDriftTimer_ = 30.0f + lcgf() * 60.0f; // change direction every 30-90s
        }
        float ddir = dirTarget_ - dir_;
        while (ddir >  PI) ddir -= PI * 2.0f;
        while (ddir < -PI) ddir += PI * 2.0f;
        dir_ += ddir * dt * 0.005f;
    }

    // Effective instantaneous speed (base + gust), 0-1
    float speed()     const { return fminf(1.0f, speed_ + gust_); }
    float direction() const { return dir_; }
    // World-space XZ components (normalized by speed)
    float dx()        const { return sinf(dir_) * speed(); }
    float dz()        const { return cosf(dir_) * speed(); }
    // Particle influence (used by rain/snow spawners as wind offset)
    float particleX() const { return dx() * 8.0f; } // m/s lateral push
    float particleZ() const { return dz() * 8.0f; }

private:
    static constexpr float PI = 3.14159265f;
    float speed_        = 0.05f;
    float dir_          = 0.0f;
    float gust_         = 0.0f;
    float gustTimer_    = 2.0f;
    float dirTarget_    = 0.0f;
    float dirDriftTimer_= 10.0f;
    uint32_t rng_       = 54321u;

    float lcgf() {
        rng_ = rng_ * 1664525u + 1013904223u;
        return (float)(rng_ >> 8) / (float)(1 << 24);
    }
};

} // namespace WM
