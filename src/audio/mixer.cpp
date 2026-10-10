#include "audio/mixer.h"
#include <algorithm>
#include <cstring>
namespace port {
static_assert(std::atomic<uint32_t>::is_always_lock_free, "audio queue needs lock-free 32-bit atomics");
void Mixer::reset() {
    head_ = 0; tail_ = 0; dropped_ = 0;
    for (auto& track : tracks_) track = {};
    ambient_ = -1; ambient_pos_ = 0; volume_ = 32768;
}
bool Mixer::enqueue(AudioCommand command) {
    const auto head = head_.load(std::memory_order_relaxed);
    if (head - tail_.load(std::memory_order_acquire) >= queue_.size()) {
        dropped_.fetch_add(1, std::memory_order_relaxed); return false;
    }
    queue_[head % queue_.size()] = command;
    head_.store(head + 1, std::memory_order_release);
    return true;
}
void Mixer::render(int16_t* stereo, int frames) {
    if (!stereo || frames <= 0) return;
    uint32_t tail = tail_.load(std::memory_order_relaxed);
    const uint32_t head = head_.load(std::memory_order_acquire);
    while (tail != head) {
        const auto command = queue_[tail++ % queue_.size()];
        if (command.track == -2) volume_ = std::clamp(command.volume, 0, 32768);
        else if (command.track == -1) { ambient_ = command.sample; ambient_pos_ = 0; }
        else if (command.track >= 0 && command.track < static_cast<int>(tracks_.size())) {
            auto& track = tracks_[command.track];
            track = {};
            if (command.sample >= 0 && command.sample < static_cast<int>(samples.size())) {
                const auto& sample = samples[command.sample];
                track.sample = command.sample;
                track.volume = std::clamp(command.volume, 0, 65535);
                track.end = static_cast<int>(sample.pcm.size() / 2);
                if (command.marker >= 0 && command.marker < static_cast<int>(sample.markers.size())) {
                    track.pos = sample.markers[command.marker][0];
                    track.end = sample.markers[command.marker][1];
                }
                if (track.pos < 0 || track.end <= track.pos || track.end > static_cast<int>(sample.pcm.size() / 2)) track = {};
            }
        }
    }
    tail_.store(tail, std::memory_order_release);
    std::memset(stereo, 0, static_cast<std::size_t>(frames) * 2 * sizeof(int16_t));
    if (ambient_ >= 0 && ambient_ < static_cast<int>(samples.size())) {
        const auto& pcm = samples[ambient_].pcm;
        const int count = static_cast<int>(pcm.size() / 2);
        if (count) for (int i = 0; i < frames; ++i) {
            stereo[2 * i] = static_cast<int16_t>((static_cast<int>(pcm[ambient_pos_ * 2]) * volume_) >> 15);
            stereo[2 * i + 1] = static_cast<int16_t>((static_cast<int>(pcm[ambient_pos_ * 2 + 1]) * volume_) >> 15);
            if (++ambient_pos_ == count) ambient_pos_ = 0;
        }
    }
    for (auto& track : tracks_) {
        if (track.sample < 0) continue;
        const auto& pcm = samples[track.sample].pcm;
        const int gain = static_cast<int>((static_cast<int64_t>(track.volume) * volume_) >> 15);
        for (int i = 0; i < frames; ++i) {
            if (track.pos >= track.end) { track.sample = -1; break; }
            for (int channel = 0; channel < 2; ++channel) {
                const int value = stereo[2 * i + channel] + static_cast<int>((static_cast<int64_t>(pcm[track.pos * 2 + channel]) * gain) / 65535);
                stereo[2 * i + channel] = static_cast<int16_t>(std::clamp(value, -32767, 32767));
            }
            ++track.pos;
        }
    }
}
}
