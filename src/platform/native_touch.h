#pragma once
#include "platform/touch_contacts.h"
namespace port {
// Native GXM owns no SDL video driver. Poll front touch explicitly and inject
// ordinary SDL finger events so the shared game input path remains unchanged.
class NativeTouch {
    bool active_ = false;
    float min_x_ = 0, min_y_ = 0, width_ = 1, height_ = 1;
    TouchContacts contacts_;

  public:
    NativeTouch() = default;
    NativeTouch(const NativeTouch &) = delete;
    NativeTouch &operator=(const NativeTouch &) = delete;
    ~NativeTouch();
    bool initialize();
    void poll();
    void reset() {
        contacts_.reset();
    }
};
} // namespace port
