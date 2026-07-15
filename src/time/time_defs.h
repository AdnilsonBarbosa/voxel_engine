#pragma once
// Shared constants and enumerations for the World Time system.
// No engine dependencies — safe to include from any header.
#include <cstdint>

namespace WT {

// ── Calendar constants ────────────────────────────────────────────────────────
static constexpr int SECS_PER_MIN    = 60;
static constexpr int SECS_PER_HOUR   = 3600;
static constexpr int SECS_PER_DAY    = 86400;  // 24 × 3600
static constexpr int DAYS_PER_WEEK   = 7;
static constexpr int DAYS_PER_MONTH  = 28;     // 4 weeks
static constexpr int MONTHS_PER_SEA  = 3;      // months per season
static constexpr int SEASONS_PER_YR  = 4;
static constexpr int MONTHS_PER_YR   = 12;     // 4 seasons × 3
static constexpr int DAYS_PER_YEAR   = 336;    // 12 × 28

// Default start: 10:00 — Day 1, Spring, Year 1
static constexpr double DEFAULT_START_SECS = 10.0 * SECS_PER_HOUR; // 36000

// Default time scale: 72 real seconds = 1 game hour → 20 real min = 1 game day.
static constexpr float  DEFAULT_TIMESCALE  = 72.0f;

// Hour thresholds for named periods (user spec: 06/08/12/18/20)
static constexpr int HR_SUNRISE   = 6;   // dawn begins
static constexpr int HR_MORNING   = 8;   // morning begins
static constexpr int HR_NOON      = 12;  // noon begins
static constexpr int HR_AFTERNOON = 14;  // afternoon begins
static constexpr int HR_EVENING   = 18;  // sunset/dusk begins
static constexpr int HR_NIGHT     = 20;  // night begins

static constexpr float WT_PI = 3.14159265358979f;

// ── Enumerations ──────────────────────────────────────────────────────────────

enum class Season : uint8_t {
    Spring  = 0,
    Summer  = 1,
    Autumn  = 2,
    Winter  = 3,
    COUNT   = 4
};

// Named time-of-day periods driven by hour thresholds above.
enum class DayPeriod : uint8_t {
    Night     = 0,  // 00:00-05:59 and 20:00-23:59
    Dawn      = 1,  // 06:00-07:59  → triggers OnSunrise
    Morning   = 2,  // 08:00-11:59
    Noon      = 3,  // 12:00-13:59  → triggers OnNoon
    Afternoon = 4,  // 14:00-17:59
    Evening   = 5,  // 18:00-19:59  → triggers OnSunset
    COUNT     = 6
};

// Events that external systems (NPCs, weather, farming, etc.) can subscribe to.
enum class TimeEvent : uint8_t {
    OnSunrise   = 0,
    OnSunset    = 1,
    OnNoon      = 2,
    OnMidnight  = 3,
    OnNewHour   = 4,
    OnNewDay    = 5,
    OnNewWeek   = 6,
    OnNewMonth  = 7,
    OnNewSeason = 8,
    OnNewYear   = 9,
    COUNT       = 10
};

// ── String helpers ────────────────────────────────────────────────────────────

inline const char* seasonName(Season s) {
    switch (s) {
        case Season::Spring: return "Spring";
        case Season::Summer: return "Summer";
        case Season::Autumn: return "Autumn";
        case Season::Winter: return "Winter";
        default: return "Unknown";
    }
}

inline const char* periodName(DayPeriod p) {
    switch (p) {
        case DayPeriod::Night:     return "Night";
        case DayPeriod::Dawn:      return "Dawn";
        case DayPeriod::Morning:   return "Morning";
        case DayPeriod::Noon:      return "Noon";
        case DayPeriod::Afternoon: return "Afternoon";
        case DayPeriod::Evening:   return "Evening";
        default: return "?";
    }
}

inline const char* dayOfWeekName(int d) {
    static const char* n[] = {
        "Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"
    };
    return (d >= 0 && d < 7) ? n[d] : "?";
}

inline const char* monthName(int m) {
    static const char* n[] = {
        "","January","February","March","April","May","June",
        "July","August","September","October","November","December"
    };
    return (m >= 1 && m <= 12) ? n[m] : "?";
}

inline const char* eventName(TimeEvent e) {
    switch (e) {
        case TimeEvent::OnSunrise:   return "OnSunrise";
        case TimeEvent::OnSunset:    return "OnSunset";
        case TimeEvent::OnNoon:      return "OnNoon";
        case TimeEvent::OnMidnight:  return "OnMidnight";
        case TimeEvent::OnNewHour:   return "OnNewHour";
        case TimeEvent::OnNewDay:    return "OnNewDay";
        case TimeEvent::OnNewWeek:   return "OnNewWeek";
        case TimeEvent::OnNewMonth:  return "OnNewMonth";
        case TimeEvent::OnNewSeason: return "OnNewSeason";
        case TimeEvent::OnNewYear:   return "OnNewYear";
        default: return "?";
    }
}

} // namespace WT
