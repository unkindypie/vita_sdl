#include "game.h"
#include "audio.h"
#include "platform/engine_hooks.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <cerrno>

extern const char* GetConfPath();

// Preserve upstream's native layout, but never partially apply an invalid file
// or load invalid bytes directly into C++ bool objects.
void ReadConf(Game* g) {
    FILE* file = std::fopen(GetConfPath(), "rb");
    if (!file) return; // First launch has no settings yet.
    Game::TalkMem talks[4]{};
    unsigned char flags[3]{};
    bool valid = std::fread(talks, sizeof(talks), 1, file) == 1 &&
                 std::fread(flags, sizeof(flags), 1, file) == 1 && std::fgetc(file) == EOF;
    std::fclose(file);
    for (const auto& talk : talks)
        valid = valid && talk.len >= 0 && talk.len < 256 && talk.buf[talk.len] == 0;
    for (auto flag : flags) valid = valid && flag <= 1;
    if (!valid) { port::log_stage("CONFIG_IGNORED invalid/truncated settings"); return; }
    std::memcpy(g->talk_mem, talks, sizeof(talks));
    g->perspective = flags[0] != 0; g->blood = flags[1] != 0; g->mute = flags[2] != 0;
    AudioMute(g->mute);
}

void WriteConf(Game* g) {
    if (!*GetConfPath()) return; // Tests may intentionally use no writable profile.
    const std::string temporary = std::string(GetConfPath()) + ".tmp";
    // Vita3K's tested file-mode translator ignores O_TRUNC. A fresh staging
    // file also avoids trailing garbage after an interrupted earlier write;
    // the committed preference file is untouched until rename succeeds.
    if (std::remove(temporary.c_str()) != 0 && errno != ENOENT) {
        port::log_stage("CONFIG_ERROR cannot clear stale staging file"); return;
    }
    FILE* file = std::fopen(temporary.c_str(), "wb");
    if (!file) { port::log_stage("CONFIG_ERROR cannot open temporary settings"); return; }
    Game::TalkMem talks[4]{};
    for (int i = 0; i < 4; ++i) {
        const int len = g->talk_mem[i].len;
        if (len < 0 || len >= 256) { std::fclose(file); std::remove(temporary.c_str()); port::log_stage("CONFIG_ERROR invalid talk history"); return; }
        talks[i].len = len;
        std::memcpy(talks[i].buf, g->talk_mem[i].buf, len);
    }
    const unsigned char flags[3] = {static_cast<unsigned char>(g->perspective), static_cast<unsigned char>(g->blood), static_cast<unsigned char>(g->mute)};
    bool ok = std::fwrite(talks, sizeof(talks), 1, file) == 1 && std::fwrite(flags, sizeof(flags), 1, file) == 1;
    if (std::fclose(file) != 0) ok = false;
    if (!ok || std::rename(temporary.c_str(), GetConfPath()) != 0) {
        std::remove(temporary.c_str()); port::log_stage("CONFIG_ERROR cannot replace settings");
    } else port::log_stage("CONFIG_SAVED settings and talk history (not world state)");
}
