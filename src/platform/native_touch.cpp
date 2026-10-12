#include "platform/native_touch.h"
#include <SDL.h>
#include <algorithm>
#ifdef __vita__
#include <psp2/touch.h>
#endif
namespace port {
NativeTouch::~NativeTouch() {
#ifdef __vita__
    if (active_)
        sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_STOP);
#endif
}
bool NativeTouch::initialize() {
#ifdef __vita__
    SceTouchPanelInfo panel{};
    if (sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &panel) < 0 || panel.maxDispX <= panel.minDispX ||
        panel.maxDispY <= panel.minDispY)
        return false;
    min_x_ = panel.minDispX;
    min_y_ = panel.minDispY;
    width_ = panel.maxDispX - panel.minDispX;
    height_ = panel.maxDispY - panel.minDispY;
    active_ = sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START) >= 0;
#endif
    return active_;
}
void NativeTouch::poll() {
#ifdef __vita__
    if (!active_)
        return;
    SceTouchData data{};
    if (sceTouchPeek(SCE_TOUCH_PORT_FRONT, &data, 1) < 1)
        return;
    TouchContacts::Sample next{};
    const unsigned count = std::min<unsigned>(data.reportNum, 6);
    auto send = [](TouchTransition transition, const TouchContact &contact) {
        const Uint32 type = transition == TouchTransition::Down     ? SDL_FINGERDOWN
                            : transition == TouchTransition::Motion ? SDL_FINGERMOTION
                                                                    : SDL_FINGERUP;
        SDL_Event event{};
        event.type = type;
        event.tfinger.type = type;
        event.tfinger.touchId = 1;
        event.tfinger.fingerId = contact.id;
        event.tfinger.x = contact.x;
        event.tfinger.y = contact.y;
        SDL_PushEvent(&event);
    };
    for (unsigned i = 0; i < count; ++i) {
        const auto &report = data.report[i];
        next[i] = {report.id, std::clamp((report.x - min_x_) / width_, 0.0F, 1.0F),
                   std::clamp((report.y - min_y_) / height_, 0.0F, 1.0F)};
    }
    contacts_.update(next, send);
#endif
}
} // namespace port
