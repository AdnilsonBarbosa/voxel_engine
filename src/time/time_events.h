#pragma once
// TimeEvents — lightweight publish/subscribe event bus for the World Time system.
// External systems (NPCs, weather, farming, economy) call subscribe() once at
// init time; TimeManager fires events on the main thread once per second.
#include "time_defs.h"
#include <functional>
#include <vector>

namespace WT {

using EventCallback = std::function<void(TimeEvent)>;

class TimeEvents {
public:
    // Register a callback for a specific event type.
    // Intended for future systems: NPCs, weather, farming, animals, economy.
    void subscribe(TimeEvent ev, EventCallback cb) {
        listeners_[(int)ev].push_back(std::move(cb));
    }

    // Broadcast an event to all registered listeners. Called by TimeManager.
    void fire(TimeEvent ev) const {
        for (auto& cb : listeners_[(int)ev]) cb(ev);
    }

    // Remove all listeners (e.g., on world unload).
    void clear() {
        for (auto& v : listeners_) v.clear();
    }

    int subscriberCount(TimeEvent ev) const {
        return (int)listeners_[(int)ev].size();
    }

    int totalSubscribers() const {
        int n = 0;
        for (auto& v : listeners_) n += (int)v.size();
        return n;
    }

private:
    std::vector<EventCallback> listeners_[(int)TimeEvent::COUNT];
};

} // namespace WT
