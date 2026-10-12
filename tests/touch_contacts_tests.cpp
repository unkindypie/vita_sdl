#include "platform/touch_contacts.h"
#include <cstdio>
#include <limits>
#include <vector>
int main() {
    port::TouchContacts tracker;
    using Transition = port::TouchTransition;
    struct Event {
        Transition type;
        port::TouchContact contact;
    };
    std::vector<Event> events;
    auto send = [&](Transition type, port::TouchContact contact) { events.push_back({type, contact}); };
    int failures = 0;
    auto check = [&](bool value, const char *message) {
        if (!value) {
            std::fprintf(stderr, "FAIL %s\n", message);
            ++failures;
        }
    };
    port::TouchContacts::Sample sample{};
    tracker.update(sample, send);
    check(events.empty(), "empty sample emits no events");
    sample[0] = {4, 0.25F, 0.5F};
    sample[1] = {2, 0.75F, 0.5F};
    tracker.update(sample, send);
    check(events.size() == 2 && events[0].type == Transition::Down && events[1].type == Transition::Down,
          "new identities press once");
    events.clear();
    std::swap(sample[0], sample[1]);
    tracker.update(sample, send);
    check(events.empty(), "reordering held contacts is not a motion or new press");
    sample[1].x = 0.5F;
    tracker.update(sample, send);
    check(events.size() == 1 && events[0].type == Transition::Motion && events[0].contact.id == 4,
          "motion follows identity");
    events.clear();
    sample[0] = {};
    tracker.update(sample, send);
    check(events.size() == 1 && events[0].type == Transition::Up && events[0].contact.id == 2,
          "missing identity releases");
    events.clear();
    sample[0] = sample[1];
    tracker.update(sample, send);
    check(events.empty(), "duplicate report does not duplicate a held contact");
    events.clear();
    tracker.reset();
    tracker.update(sample, send);
    check(events.size() == 1 && events[0].type == Transition::Down,
          "focus reset allows a held finger to press again");
    events.clear();
    sample = {};
    sample[0] = {9, -1, 2};
    sample[1] = {10, std::numeric_limits<float>::quiet_NaN(), 0};
    tracker.update(sample, send);
    check(events.size() == 2 && events[0].type == Transition::Up && events[1].type == Transition::Down &&
              events[1].contact.x == 0 && events[1].contact.y == 1,
          "release old slots before new press; clamp coordinates and reject nonfinite report");
    return failures ? 1 : 0;
}
