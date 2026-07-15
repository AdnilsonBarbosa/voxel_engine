#pragma once
// GameCalendar — computes the full calendar state (day/week/month/year/season)
// from a raw total-seconds counter.  Updated once per real second for mobile.
#include "time_defs.h"

namespace WT {

// Full snapshot of the calendar at a given moment.
// Recomputed from totalGameSecs once per second; used by debug HUD and
// future systems: farming (season), NPCs (schedule), economy (market day), etc.
struct CalendarState {
    int       second     = 0;               // 0–59
    int       minute     = 0;               // 0–59
    int       hour       = HR_SUNRISE;      // 0–23
    int       dayOfWeek  = 0;               // 0=Monday … 6=Sunday
    int       dayOfMonth = 1;               // 1–28
    int       weekOfMonth= 1;               // 1–4
    int       month      = 1;               // 1–12
    int       year       = 1;               // 1 …
    Season    season     = Season::Spring;
    DayPeriod period     = DayPeriod::Dawn;
    long long totalDays  = 0;               // absolute day index (day 1 = index 0)
    long long weekNum    = 0;               // absolute week index (for OnNewWeek)

    // Derive a complete CalendarState from total accumulated game seconds.
    // Pure function — no side-effects, cheap (integer arithmetic only).
    static CalendarState from(double totalGameSecs) {
        CalendarState c;
        long long S = (long long)totalGameSecs;  // truncate to integer seconds

        // ── Time of day ──────────────────────────────────────────────────────
        int todSec   = (int)(S % SECS_PER_DAY);
        c.second     = todSec % SECS_PER_MIN;
        c.minute     = (todSec / SECS_PER_MIN) % SECS_PER_MIN;
        c.hour       = todSec / SECS_PER_HOUR;

        // ── Calendar ─────────────────────────────────────────────────────────
        c.totalDays  = S / SECS_PER_DAY;
        c.weekNum    = c.totalDays / DAYS_PER_WEEK;

        long long dayOfYear = c.totalDays % DAYS_PER_YEAR;
        c.year       = (int)(c.totalDays / DAYS_PER_YEAR) + 1;
        c.month      = (int)(dayOfYear / DAYS_PER_MONTH) + 1;  // 1–12
        c.dayOfMonth = (int)(dayOfYear % DAYS_PER_MONTH) + 1;  // 1–28
        c.weekOfMonth= (c.dayOfMonth - 1) / DAYS_PER_WEEK + 1; // 1–4
        c.dayOfWeek  = (int)(c.totalDays % DAYS_PER_WEEK);     // 0=Monday

        // ── Season (3 months each, mapping: Jan–Mar=Spring, …, Oct–Dec=Winter)
        c.season = (Season)(((c.month - 1) / MONTHS_PER_SEA) % (int)Season::COUNT);

        // ── Day period (user spec: 06/08/12/18/20 hour thresholds) ───────────
        if      (c.hour < HR_SUNRISE)   c.period = DayPeriod::Night;
        else if (c.hour < HR_MORNING)   c.period = DayPeriod::Dawn;
        else if (c.hour < HR_NOON)      c.period = DayPeriod::Morning;
        else if (c.hour < HR_AFTERNOON) c.period = DayPeriod::Noon;
        else if (c.hour < HR_EVENING)   c.period = DayPeriod::Afternoon;
        else if (c.hour < HR_NIGHT)     c.period = DayPeriod::Evening;
        else                            c.period = DayPeriod::Night;

        return c;
    }
};

// GameCalendar wraps CalendarState and provides semantic accessors.
// Designed for future NPC/farming/economy queries:
//   calendar.isMarketDay()  → dayOfWeek == 5 (Saturday)
//   calendar.isHarvest()    → season == Autumn && dayOfMonth >= 20
class GameCalendar {
public:
    void update(double totalGameSecs) {
        state_ = CalendarState::from(totalGameSecs);
    }

    const CalendarState& state() const { return state_; }

    // ── Convenience accessors (for NPC/farming/economy hooks) ────────────────
    bool isDay()        const { return state_.period != DayPeriod::Night; }
    bool isNight()      const { return state_.period == DayPeriod::Night; }
    bool isWeekend()    const { return state_.dayOfWeek >= 5; }   // Sat / Sun
    bool isMorketDay()  const { return state_.dayOfWeek == 5; }   // Saturday
    bool isLastDayOfMonth() const { return state_.dayOfMonth == DAYS_PER_MONTH; }
    bool isSeasonEnd()  const {
        return state_.month % MONTHS_PER_SEA == 0 && isLastDayOfMonth();
    }

    // Future: farming — crops grow in Spring/Summer, wither in Winter
    bool isCropSeason() const {
        return state_.season == Season::Spring || state_.season == Season::Summer;
    }

    // Format for debug / HUD
    void format(char* buf, int size) const {
        const CalendarState& c = state_;
        snprintf(buf, size,
            "%s %d %s %d (Week %d) | %s | %02d:%02d | %s",
            dayOfWeekName(c.dayOfWeek), c.dayOfMonth,
            monthName(c.month), c.year, c.weekOfMonth,
            seasonName(c.season), c.hour, c.minute,
            periodName(c.period));
    }

private:
    CalendarState state_;
};

} // namespace WT
