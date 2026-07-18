#pragma once
// TimeManager — master controller for the World Time system.
//
// Responsibilities:
//   • Advance game time every frame (realDt × timeScale)
//   • Update calendar once per real second (mobile-friendly: cheap CPU)
//   • Fire events (OnSunrise, OnNewDay, OnNewSeason, …) at correct instants
//   • Expose sun direction + brightness for the sky/terrain shaders
//   • Save / load state (world_time.dat) so time persists across sessions
//
// Usage (main.cpp):
//   WT::TimeManager wt;
//   wt.load("world_time.dat");
//   // subscribe events for NPCs, weather, etc.
//   wt.events().subscribe(WT::TimeEvent::OnNewDay, [](auto){ ... });
//   // each frame:
//   wt.update(dt);
//   float sun[3]; wt.sunDir(sun);
//   float sunLight = wt.sunLight();
//
#include "time_defs.h"
#include "time_events.h"
#include "game_calendar.h"
#include "world_clock.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include "SDL.h"   // SDL_Log

namespace WT {

class TimeManager {
public:
    // ── Construction ──────────────────────────────────────────────────────────
    TimeManager() {
        clock_.setTotalSecs(DEFAULT_START_SECS);
        calendar_.update(DEFAULT_START_SECS);
        prev_ = calendar_.state();
    }

    // ── Frame update ──────────────────────────────────────────────────────────
    // Call every frame with real elapsed seconds.  Only the continuous sun arc
    // updates every frame; calendar + events fire once per real second.
    void update(float realDt) {
        // Advance game clock (smooth, every frame)
        double advance = (double)realDt * (double)timeScale_;
        clock_.setTotalSecs(clock_.totalSecs() + advance);

        // Once per real second: recompute calendar + fire events (mobile-friendly)
        accum_ += realDt;
        if (accum_ >= 1.0f) {
            accum_ -= 1.0f;
            CalendarState before = calendar_.state();
            calendar_.update(clock_.totalSecs());
            fireEvents_(before, calendar_.state());
        }

        // Auto-save every 60 real seconds
        saveAccum_ += realDt;
        if (saveAccum_ >= 60.0f && savePath_[0]) {
            saveAccum_ = 0.0f;
            save(savePath_);
        }
    }

    // ── Shader inputs (updated every frame for smooth sun movement) ───────────
    float sunLight()               const { return clock_.sunLight(); }
    void  sunDir(float out[3])     const { clock_.sunDir(out); }
    float sunAngle()               const { return clock_.sunAngle(); }
    float dayProgress()            const { return clock_.dayProgress(); }

    // ── Calendar accessors ────────────────────────────────────────────────────
    const CalendarState& calendar() const { return calendar_.state(); }
    int   hour()         const { return calendar_.state().hour; }
    int   minute()       const { return calendar_.state().minute; }
    int   dayOfMonth()   const { return calendar_.state().dayOfMonth; }
    int   month()        const { return calendar_.state().month; }
    int   year()         const { return calendar_.state().year; }
    Season   season()    const { return calendar_.state().season; }
    DayPeriod period()   const { return calendar_.state().period; }
    bool  isDay()        const { return calendar_.isDay(); }
    bool  isNight()      const { return calendar_.isNight(); }

    // ── Event bus ─────────────────────────────────────────────────────────────
    TimeEvents& events()       { return events_; }
    const TimeEvents& events() const { return events_; }

    // ── Time scale ────────────────────────────────────────────────────────────
    float timeScale()           const { return timeScale_; }
    void  setTimeScale(float s)       { timeScale_ = (s > 0.0f) ? s : DEFAULT_TIMESCALE; }

    double totalGameSecs() const { return clock_.totalSecs(); }

    // Keep the loaded DAY (and therefore season/year) but reset the clock to a
    // fixed hour — sessions resume on the same calendar day at a bright hour.
    void normalizeToHour(int hour) {
        const long long days = (long long)(clock_.totalSecs() / SECS_PER_DAY);
        const double t = (double)days * SECS_PER_DAY + (double)hour * SECS_PER_HOUR;
        clock_.setTotalSecs(t);
        calendar_.update(t);
        prev_ = calendar_.state();
    }

    // ── Save / Load ───────────────────────────────────────────────────────────
    // Binary format (20 bytes):  magic[4] | version(u32) | totalSecs(d64) | scale(f32)
    bool load(const char* path) {
        strncpy(savePath_, path, sizeof(savePath_) - 1);
        savePath_[sizeof(savePath_) - 1] = 0;

        FILE* f = fopen(path, "rb");
        if (!f) return false;    // first run — defaults used

        struct Hdr { char magic[4]; uint32_t version; double totalSecs; float scale; };
        Hdr h;
        bool ok = (fread(&h, sizeof(h), 1, f) == 1)
               && (h.magic[0]=='T' && h.magic[1]=='I' && h.magic[2]=='M' && h.magic[3]=='E')
               && (h.version == 1u);
        fclose(f);

        if (ok) {
            clock_.setTotalSecs(h.totalSecs);
            timeScale_ = (h.scale > 0.0f) ? h.scale : DEFAULT_TIMESCALE;
            calendar_.update(h.totalSecs);
            prev_ = calendar_.state();
            SDL_Log("[WorldTime] Loaded: %02d:%02d Day %d %s Year %d | scale=%.0f",
                    calendar().hour, calendar().minute,
                    calendar().dayOfMonth, seasonName(calendar().season),
                    calendar().year, (double)timeScale_);
        }
        return ok;
    }

    bool save(const char* path) const {
        FILE* f = fopen(path, "wb");
        if (!f) return false;
        struct Hdr { char magic[4]; uint32_t version; double totalSecs; float scale; };
        Hdr h = { {'T','I','M','E'}, 1u, clock_.totalSecs(), timeScale_ };
        bool ok = (fwrite(&h, sizeof(h), 1, f) == 1);
        fclose(f);
        return ok;
    }

    // Force-save to the path used by load() (convenience, e.g., on exit).
    bool saveToLoadPath() const {
        return savePath_[0] ? save(savePath_) : false;
    }

    // ── Debug ─────────────────────────────────────────────────────────────────
    void debugLog() const {
        const CalendarState& c = calendar_.state();
        SDL_Log("[WorldTime] %02d:%02d:%02d %s | %s %d %s %d (Week %d) | %s | scale=%.0fx | totalSecs=%.0f",
                c.hour, c.minute, c.second, periodName(c.period),
                dayOfWeekName(c.dayOfWeek), c.dayOfMonth,
                monthName(c.month), c.year, c.weekOfMonth,
                seasonName(c.season), (double)timeScale_, clock_.totalSecs());
    }

    void debugString(char* buf, int size) const {
        const CalendarState& c = calendar_.state();
        snprintf(buf, size,
            "%02d:%02d %s | Day %d (%s) | %s %s | Year %d | x%.0f",
            c.hour, c.minute, periodName(c.period),
            c.dayOfMonth, dayOfWeekName(c.dayOfWeek),
            monthName(c.month), seasonName(c.season),
            c.year, (double)timeScale_);
    }

private:
    WorldClock    clock_;
    GameCalendar  calendar_;
    CalendarState prev_;         // previous second's state for event detection
    TimeEvents    events_;

    float  timeScale_  = DEFAULT_TIMESCALE;
    float  accum_      = 0.0f;   // real-seconds accumulator (throttle to 1 Hz)
    float  saveAccum_  = 0.0f;   // seconds since last auto-save
    char   savePath_[256] = {};

    // ── Event detection (called once per real second) ─────────────────────────
    void fireEvents_(const CalendarState& p, const CalendarState& c) {
        // Per-hour events
        if (p.hour != c.hour) {
            events_.fire(TimeEvent::OnNewHour);
            if (c.hour ==  0) events_.fire(TimeEvent::OnMidnight);
            if (c.hour == HR_NOON) events_.fire(TimeEvent::OnNoon);
        }

        // Period transitions → Sunrise / Sunset
        if (p.period != DayPeriod::Dawn   && c.period == DayPeriod::Dawn)
            events_.fire(TimeEvent::OnSunrise);
        if (p.period != DayPeriod::Evening && c.period == DayPeriod::Evening)
            events_.fire(TimeEvent::OnSunset);

        // Day / week / month / season / year rollovers
        if (p.totalDays != c.totalDays) {
            events_.fire(TimeEvent::OnNewDay);
        }
        if (p.weekNum != c.weekNum) {
            events_.fire(TimeEvent::OnNewWeek);
        }
        if (p.month != c.month) {
            events_.fire(TimeEvent::OnNewMonth);
            if (p.season != c.season) events_.fire(TimeEvent::OnNewSeason);
        }
        if (p.year != c.year) {
            events_.fire(TimeEvent::OnNewYear);
        }

        prev_ = c;   // keep in sync for next tick
    }
};

} // namespace WT
