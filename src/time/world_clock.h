#pragma once
// WorldClock — tracks continuous game time (as a double of seconds) and
// derives smooth per-frame sun/moon orientation for the sky renderer.
// Updated every frame (not throttled) so the sun arc is perfectly smooth.
#include "time_defs.h"
#include <cmath>

namespace WT {

class WorldClock {
public:
    // ── State ─────────────────────────────────────────────────────────────────
    double totalSecs()  const { return totalSecs_; }
    void   setTotalSecs(double s) { totalSecs_ = s; }

    // ── Sun / Moon geometry ───────────────────────────────────────────────────
    // Sun angle in radians around the east–west axis.
    //   −π/2  (midnight)   → sun directly below
    //     0   (06:00)      → sunrise, sun at horizon east
    //   +π/2  (12:00)      → sun at zenith
    //   +π    (18:00)      → sunset, sun at horizon west
    //   +3π/2 (24:00/0:00) → sun directly below again
    float sunAngle() const {
        double tod = fmod(totalSecs_, (double)SECS_PER_DAY);
        return (float)(2.0 * WT_PI * tod / SECS_PER_DAY - WT_PI / 2.0);
    }

    // Vertical sine of the sun angle: −1 = midnight, 0 = horizon, +1 = noon.
    float sunElevation() const { return sinf(sunAngle()); }

    // Fill a normalised 3-vector for the sky shader's uSunDir.
    // x = east–west, y = up, z = slight fixed depth so the disc doesn't flip.
    void sunDir(float out[3]) const {
        float a  = sunAngle();
        out[0]   = cosf(a);
        out[1]   = sinf(a);
        out[2]   = 0.35f;
        float l  = sqrtf(out[0]*out[0] + out[1]*out[1] + out[2]*out[2]);
        if (l > 0.0f) { out[0] /= l; out[1] /= l; out[2] /= l; }
    }

    // Sun brightness for the terrain/block shader uSunLight.
    //   Night: 0.35 (cave ambient floor)
    //   Noon:  1.00 (full brightness)
    float sunLight() const {
        float e = sunElevation();
        return 0.35f + 0.65f * (e > 0.0f ? e : 0.0f);
    }

    // 0.0 = midnight, 0.5 = noon, 1.0 = next midnight (smooth 0–1 progress)
    float dayProgress() const {
        double tod = fmod(totalSecs_, (double)SECS_PER_DAY);
        return (float)(tod / SECS_PER_DAY);
    }

    // ── Fractional hours / minutes for HUD display ────────────────────────────
    float floatHour() const {
        double tod = fmod(totalSecs_, (double)SECS_PER_DAY);
        return (float)(tod / SECS_PER_HOUR);
    }

private:
    double totalSecs_ = DEFAULT_START_SECS;
};

} // namespace WT
