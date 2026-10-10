#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
namespace port {
struct Sample {
    std::vector<int16_t> pcm; // 44,100 Hz, signed native-endian stereo
    std::vector<std::array<int, 2>> markers; // frames, exclusive end
};
struct AudioCommand { int track = -1, sample = -1, volume = 0, marker = -1; };
class Mixer {
public:
    std::array<Sample, 2> samples;
    bool enqueue(AudioCommand command); // one producer: game thread
    void render(int16_t* stereo, int frames); // one consumer: audio callback
    void reset(); // only while the device is closed/paused and synchronized
    uint32_t dropped() const { return dropped_.load(); }
private:
    struct Track { int sample = -1, pos = 0, end = 0, volume = 0; };
    std::array<Track, 16> tracks_{};
    std::array<AudioCommand, 128> queue_{};
    std::atomic<uint32_t> head_{0}, tail_{0}, dropped_{0};
    int ambient_ = -1, ambient_pos_ = 0, volume_ = 32768;
};
bool load_sample(const std::string& path, Sample& sample, std::string& error);
bool audio_ready();
void pause_audio(bool paused);
uint32_t audio_dropped();
uint32_t audio_callback_count();
uint32_t audio_signal_count();
}
