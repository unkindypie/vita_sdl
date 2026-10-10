#include "audio/mixer.h"
#include "audio.h"
#include "platform/engine_hooks.h"
#include <SDL2/SDL.h>
#include "stb_vorbis.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <algorithm>
#include <fstream>
#include <iterator>
extern char base_path[];
namespace {
port::Mixer mixer;
SDL_AudioDeviceID device = 0;
int next_track = 0, last_foot = 0;
std::atomic<uint32_t> callbacks{0}, signal_callbacks{0};
void callback(void*, Uint8* stream, int bytes) {
    mixer.render(reinterpret_cast<int16_t*>(stream), bytes / 4);
    callbacks.fetch_add(1, std::memory_order_relaxed);
    const auto* pcm = reinterpret_cast<const int16_t*>(stream);
    for (int i = 0; i < bytes / 2; ++i) if (pcm[i]) {
        signal_callbacks.fetch_add(1, std::memory_order_relaxed); break;
    }
}
}
namespace port {
bool load_sample(const std::string& path, Sample& sample, std::string& error) {
    sample = {};
    int code = 0;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() <= 0 || file.tellg() > 8 * 1024 * 1024) { error = "Missing/oversized audio: " + path; return false; }
    file.seekg(0);
    const std::vector<uint8_t> encoded{std::istreambuf_iterator<char>(file), {}};
    std::unique_ptr<stb_vorbis, decltype(&stb_vorbis_close)> decoder(stb_vorbis_open_memory(encoded.data(), static_cast<int>(encoded.size()), &code, nullptr), stb_vorbis_close);
    if (!decoder) { error = "Cannot decode Ogg: " + path; return false; }
    const auto info = stb_vorbis_get_info(decoder.get());
    const unsigned frames = stb_vorbis_stream_length_in_samples(decoder.get());
    if (info.sample_rate != 44100 || info.channels < 1 || info.channels > 2 || !frames || frames > 44100 * 300) {
        error = "Unsupported/oversized audio: " + path; return false;
    }
    sample.pcm.resize(static_cast<std::size_t>(frames) * 2);
    unsigned decoded = 0;
    float** channels = nullptr;
    int channel_count = 0, block = 0;
    while ((block = stb_vorbis_get_frame_float(decoder.get(), &channel_count, &channels)) > 0) {
        for (int i = 0; i < block && decoded < frames; ++i, ++decoded)
            for (int channel = 0; channel < 2; ++channel) {
                const float value = channels[std::min(channel, channel_count - 1)][i];
                sample.pcm[decoded * 2 + channel] = static_cast<int16_t>(std::clamp(static_cast<int>(value * 32767), -32767, 32767));
            }
    }
    if (decoded != frames) { error = "Truncated audio: " + path; sample = {}; return false; }
    const char* markers = stb_vorbis_get_markers(decoder.get());
    while (markers && *markers) {
        double begin = 0, end = 0;
        if (std::sscanf(markers, "%lf\t%lf", &begin, &end) == 2) {
            if (!std::isfinite(begin) || !std::isfinite(end) || begin < 0 || end <= begin || end * 44100 > frames + 0.5) {
                error = "Invalid Ogg cue markers: " + path; sample = {}; return false;
            }
            sample.markers.push_back({static_cast<int>(std::floor(begin * 44100 + .5)), static_cast<int>(std::floor(end * 44100 + .5))});
        }
        markers = std::strchr(markers, '\n');
        if (markers) ++markers;
    }
    return true;
}
bool audio_ready() { return device != 0; }
void pause_audio(bool paused) { if (device) SDL_PauseAudioDevice(device, paused ? 1 : 0); }
uint32_t audio_dropped() { return mixer.dropped(); }
uint32_t audio_callback_count() { return callbacks.load(std::memory_order_relaxed); }
uint32_t audio_signal_count() { return signal_callbacks.load(std::memory_order_relaxed); }
}
bool InitAudio() {
    FreeAudio();
    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        port::log_stage((std::string("AUDIO_UNAVAILABLE ") + SDL_GetError()).c_str()); return false;
    }
    std::string error;
    for (int i = 0; i < 2; ++i) {
        if (!port::load_sample(std::string(base_path) + "samples/" + (i == 0 ? "forest.ogg" : "footsteps.ogg"), mixer.samples[i], error)) {
            port::log_stage(("AUDIO_ERROR " + error).c_str()); return false;
        }
    }
    SDL_AudioSpec wanted{}, actual{};
    wanted.freq = 44100; wanted.format = AUDIO_S16SYS; wanted.channels = 2;
    wanted.samples = 1024; wanted.callback = callback;
    device = SDL_OpenAudioDevice(nullptr, 0, &wanted, &actual, 0);
    if (!device) { port::log_stage((std::string("AUDIO_UNAVAILABLE ") + SDL_GetError()).c_str()); return false; }
    mixer.enqueue({-1, 0, 32768, -1});
    SDL_PauseAudioDevice(device, 0);
    port::log_stage("AUDIO_READY 44100Hz stereo S16 tracks=16 queue=128");
    return true;
}
void FreeAudio() {
    if (device) SDL_CloseAudioDevice(device);
    device = 0; mixer.reset();
    for (auto& sample : mixer.samples) sample = {};
    next_track = 0; last_foot = 0;
    callbacks = 0; signal_callbacks = 0;
}
int GetSampleID(AUDIO_FILE file) { return file >= FOREST && file < AUDIO_FILES ? static_cast<int>(file) : -1; }
void CallAudio(const uint8_t* data, int size) {
    if (!device || !data) return;
    if (size == 2) { uint16_t gain; std::memcpy(&gain, data, 2); mixer.enqueue({-2, -1, gain, -1}); }
    else if (size == 4) { int32_t id; std::memcpy(&id, data, 4); mixer.enqueue({-1, id, 0, -1}); }
    else if (size == 12 || size == 16) {
        int32_t command[4] = {0, 0, 0, -1}; std::memcpy(command, data, size);
        mixer.enqueue({command[0], command[1], command[2], command[3]});
    }
}
void AudioMute(bool muted) { const uint16_t volume = muted ? 0 : 32768; CallAudio(reinterpret_cast<const uint8_t*>(&volume), 2); }
void AudioWalk(int foot, int volume, const SpriteReq*, int material) {
    if (!foot) { last_foot ^= 1; foot = last_foot; }
    const int32_t command[4] = {next_track, FOOTSTEPS, volume, 2 * material + (foot & 1)};
    next_track = (next_track + 1) % 16;
    CallAudio(reinterpret_cast<const uint8_t*>(command), sizeof(command));
}
