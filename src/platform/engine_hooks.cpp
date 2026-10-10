// Explicit Phase 0-2 adapters. Full game scripting/audio/networking are deferred.
#include "game.h"
#include "game_api.h"
#include "audio.h"
#include "render.h"
#include <cstdio>
#ifdef __vita__
#include <psp2/io/fcntl.h>
#include <cstring>
#endif
char base_path[1024] = "./";
Game* game = nullptr;
Terrain* terrain = nullptr;
World* world = nullptr;
Server* server = nullptr;
Material mat[256] = {};
void* akAPI_Buff = nullptr;
uint64_t (*MakeStamp)() = nullptr;
static bool exit_requested = false;
static bool fullscreen = false;
static char config_path[1024] = {};
namespace port {
void set_config_path(const char* path) { std::snprintf(config_path, sizeof(config_path), "%s", path); }
bool should_exit() { return exit_requested; }
void reset_exit() { exit_requested = false; }
void log_stage(const char* stage) {
    std::printf("STAGE %s\n", stage);
#ifdef __vita__
    const int fd = sceIoOpen("ux0:data/asciicker/stages.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, stage, std::strlen(stage));
        sceIoWrite(fd, "\n", 1);
        sceIoClose(fd);
    }
#endif
}
}
void* GetMaterialArr() { return mat; }
void Buzz() {}
void SyncConf() {}
const char* GetConfPath() { return config_path; }
void exit_handler(int) { exit_requested = true; }
bool NextGLFont() { return false; }
bool PrevGLFont() { return false; }
void ToggleFullscreen(Game*) { fullscreen = !fullscreen; }
bool IsFullscreen(Game*) { return fullscreen; }
bool Server::Send(const uint8_t*, int) { return false; }
void Server::Proc() {}
void Server::Log(const char* text) { std::fprintf(stderr, "OFFLINE: %s\n", text); }
bool InitAudio() { return false; }
void FreeAudio() {}
void CallAudio(const uint8_t*, int) {}
void AudioWalk(int, int, const SpriteReq*, int) {}
void AudioMute(bool) {}
int GetSampleID(AUDIO_FILE) { return -1; }
bool akAPI_OnSay(const char*, int, bool*) { return false; }
bool akAPI_OnItem(int, int, int, int, int, const char*, bool*, int*, const char**) { return false; }
bool akAPI_OnFrame() { return false; }
void akAPI_Exec(const char*, int, bool) { std::fprintf(stderr, "SCRIPT_UNAVAILABLE: Phase 3 bindings deferred\n"); }
void akAPI_CB(int) {}
void akAPI_Init() {}
void akAPI_Free() {}
