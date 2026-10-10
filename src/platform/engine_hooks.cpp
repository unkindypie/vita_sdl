// Native platform hooks; networking deliberately remains offline.
#include <SDL2/SDL.h>
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
uint64_t (*MakeStamp)() = nullptr;
static bool exit_requested = false;
static bool new_game_requested = false;
static char config_path[1024] = {};
namespace port {
void set_config_path(const char* path) { std::snprintf(config_path, sizeof(config_path), "%s", path); }
bool should_exit() { return exit_requested; }
void reset_exit() { exit_requested = false; }
void request_new_game() { new_game_requested = true; }
bool take_new_game_request() { const bool pending = new_game_requested; new_game_requested = false; return pending; }
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
void PortableNewGame() { port::request_new_game(); }
bool NextGLFont() { return false; }
bool PrevGLFont() { return false; }
void ToggleFullscreen(Game*) {
#ifndef __vita__
    SDL_Window* window = SDL_GetKeyboardFocus();
    if (window && SDL_SetWindowFullscreen(window, (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
        port::log_stage(SDL_GetError());
#endif
}
bool IsFullscreen(Game*) {
#ifdef __vita__
    return true;
#else
    SDL_Window* window = SDL_GetKeyboardFocus();
    return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP);
#endif
}
bool Server::Send(const uint8_t*, int) { return false; }
void Server::Proc() {}
void Server::Log(const char* text) { std::fprintf(stderr, "OFFLINE: %s\n", text); }
