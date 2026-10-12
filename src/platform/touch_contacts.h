#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace port {
struct TouchContact {
    int id = -1;
    float x = 0, y = 0;
};
enum class TouchTransition { Down, Motion, Up };

// Identity-based, bounded contact diff. Poll order may change between samples.
// No SDL/native types or allocation: host tests exercise the native event rules.
class TouchContacts {
  public:
    static constexpr std::size_t kCapacity = 6;
    using Sample = std::array<TouchContact, kCapacity>;
    void reset() {
        contacts_ = {};
    }

    template <class Sink> void update(Sample next, Sink send) {
        for (std::size_t i = 0; i < next.size(); ++i) {
            auto &contact = next[i];
            if (contact.id < 0 || !std::isfinite(contact.x) || !std::isfinite(contact.y)) {
                contact = {};
                continue;
            }
            const auto duplicate =
                std::find_if(next.begin(), next.begin() + i,
                             [&](const TouchContact &other) { return other.id == contact.id; });
            if (duplicate != next.begin() + i) {
                contact = {};
                continue;
            }
            contact.x = std::clamp(contact.x, 0.0F, 1.0F);
            contact.y = std::clamp(contact.y, 0.0F, 1.0F);
        }
        // Release first: bounded downstream finger slots must become available
        // before newly appearing identities claim them in this same poll.
        for (const auto &contact : contacts_) {
            if (contact.id < 0)
                continue;
            const auto found = std::find_if(
                next.begin(), next.end(), [&](const TouchContact &other) { return other.id == contact.id; });
            if (found == next.end())
                send(TouchTransition::Up, contact);
        }
        for (const auto &contact : next) {
            if (contact.id < 0)
                continue;
            const auto old = std::find_if(contacts_.begin(), contacts_.end(),
                                          [&](const TouchContact &other) { return other.id == contact.id; });
            if (old == contacts_.end())
                send(TouchTransition::Down, contact);
            else if (old->x != contact.x || old->y != contact.y)
                send(TouchTransition::Motion, contact);
        }
        contacts_ = next;
    }

  private:
    Sample contacts_{};
};
} // namespace port
