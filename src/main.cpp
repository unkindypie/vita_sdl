#include <SDL2/SDL.h>
#include "app/scene.h"
#include "app/self_test.h"
#include "platform/engine_hooks.h"
#include "platform/input.h"
#include "scripting/probe.h"
#include "scripting/runtime.h"
#include "audio/mixer.h"
#include "audio.h"
#include "ui/cells.h"
#include "platform.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
unsigned int _newlib_heap_size_user = 192 * 1024 * 1024;
unsigned int sceUserMainThreadStackSize = 2 * 1024 * 1024;
#endif

namespace {
constexpr int kColumns = 120, kRows = 68, kWidth = 960, kHeight = 544;
uint64_t now_us() {
    const uint64_t ticks = SDL_GetPerformanceCounter();
    const uint64_t frequency = SDL_GetPerformanceFrequency();
    return (ticks / frequency) * 1000000 + (ticks % frequency) * 1000000 / frequency;
}
struct Options {
    std::string assets, manifest, screenshot;
    int frames = 0;
    int seconds = 0;
    bool smoke = false;
    bool probe = false;
    bool no_js = false;
    bool verify_gameplay = false;
    std::string script;
};
bool options(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--smoke") out.smoke = true;
        else if (arg == "--no-js") out.no_js = true;
        else if (arg == "--verify-gameplay") out.verify_gameplay = true;
        else if (arg == "--probe-js") out.probe = true;
        else if ((arg == "--script" || arg == "--assets" || arg == "--manifest" || arg == "--frames" || arg == "--seconds" || arg == "--screenshot") && i + 1 < argc) {
            const char* value = argv[++i];
            if (arg == "--script") out.script = value;
            if (arg == "--assets") out.assets = value;
            if (arg == "--manifest") out.manifest = value;
            if (arg == "--screenshot") out.screenshot = value;
            if (arg == "--frames" || arg == "--seconds") {
                char* end = nullptr;
                const long count = std::strtol(value, &end, 10);
                if (*end || count < 1 || count > 1000000) return false;
                if (arg == "--frames") out.frames = static_cast<int>(count);
                else out.seconds = static_cast<int>(count);
            }
        } else return false;
    }
    return (!out.smoke || out.frames >= 180 || out.seconds > 0) && (!out.verify_gameplay || (out.smoke && !out.no_js)) && (!out.no_js || out.script.empty());
}
struct SdlLifetime { ~SdlLifetime() { SDL_Quit(); } };
struct SceneLifetime { ~SceneLifetime() { port::close_scene(); } };
double percentile(std::vector<double> values, double fraction) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    return values[static_cast<std::size_t>((values.size() - 1) * fraction)];
}
long resident_kib() {
#ifndef __vita__
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        long value = 0;
        if (std::sscanf(line.c_str(), "VmRSS: %ld kB", &value) == 1) return value;
    }
#endif
    return -1;
}
}

int main(int argc, char** argv) {
#ifdef __vita__
    sceIoMkdir("ux0:data/asciicker", 0777);
    std::freopen("ux0:data/asciicker/port.log", "w", stdout);
    std::freopen("ux0:data/asciicker/error.log", "w", stderr);
#endif
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::puts("BOOT_READY Asciicker Vita offline runtime");
    port::log_stage("boot.ready");
    Options opt;
    if (!options(argc, argv, opt)) {
        std::fprintf(stderr, "Usage: vita_sdl2 [--assets DIR] [--manifest FILE] [--frames N] [--seconds N] [--smoke --frames 180+] [--verify-gameplay] [--script FILE | --no-js] [--screenshot FILE.bmp] [--probe-js]\n");
        return 2;
    }
    std::string error;
    if (opt.probe) {
        if (!port::probe_quickjs(error)) { std::fprintf(stderr, "%s\n", error.c_str()); return 1; }
        std::puts("QUICKJS_PROBE_OK eval native-call shared-buffer exception teardown");
        return 0;
    }
    std::puts("SDL_INIT_BEGIN video controller timer");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1;
    }
    SdlLifetime sdl_lifetime;
    std::puts("SDL_INIT_OK");
    port::log_stage("sdl.ready");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
#ifdef __vita__
    if (opt.assets.empty()) opt.assets = "app0:assets";
    if (opt.manifest.empty()) opt.manifest = "app0:assets/manifest.txt";
    port::set_config_path("ux0:data/asciicker/asciicker.cfg");
#else
    std::unique_ptr<char, decltype(&SDL_free)> executable_dir(SDL_GetBasePath(), SDL_free);
    if (opt.assets.empty()) opt.assets = executable_dir ? std::string(executable_dir.get()) + "../../assets" : "assets";
    if (opt.manifest.empty()) opt.manifest = executable_dir ? std::string(executable_dir.get()) + "asset_manifest.txt" : "build/linux/asset_manifest.txt";
    std::unique_ptr<char, decltype(&SDL_free)> prefs(SDL_GetPrefPath("asciicker", "vita-port"), SDL_free);
    if (prefs) port::set_config_path((std::string(prefs.get()) + "asciicker.cfg").c_str());
#endif
    if (!port::validate_assets(opt.assets, opt.manifest, error)) {
        std::fprintf(stderr, "ASSET_ERROR %s\n", error.c_str()); return 1;
    }
    port::Font font{};
    if (!port::load_font(opt.assets + "/fonts/cp437_8x8.png", font, error)) {
        std::fprintf(stderr, "FONT_ERROR %s\n", error.c_str()); return 1;
    }
    using Window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
    using Renderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;
    using Texture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;
    using Controller = std::unique_ptr<SDL_GameController, decltype(&SDL_GameControllerClose)>;
    Window window(SDL_CreateWindow("Asciicker Vita | QuickJS + SDL2 | F1 menu, Tab chat/scripts",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kWidth, kHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE), SDL_DestroyWindow);
    if (!window) { std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    Renderer renderer(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC), SDL_DestroyRenderer);
    if (!renderer) renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE));
    if (!renderer) { std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(renderer.get(), kWidth, kHeight);
    Texture texture(SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight), SDL_DestroyTexture);
    if (!texture) { std::fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError()); return 1; }
    SDL_RendererInfo info{};
    SDL_GetRendererInfo(renderer.get(), &info);
    std::printf("SDL_READY driver=%s renderer=%s grid=%dx%d pixels=%dx%d\n", SDL_GetCurrentVideoDriver(), info.name, kColumns, kRows, kWidth, kHeight);
    MakeStamp = now_us;
    port::reset_exit();
    if (!port::load_scene(opt.assets, opt.smoke ? 1000000 : now_us(), error)) {
        std::fprintf(stderr, "SCENE_ERROR %s\n", error.c_str()); return 1;
    }
    SceneLifetime scene_lifetime;
    InitAudio(); // Device failure is visible and degrades to silent gameplay.
    AudioMute(game->mute);
    if (opt.verify_gameplay) AudioMute(false);
    if (!opt.script.empty() && !port::load_script(opt.script, error)) {
        port::log_stage(("SCRIPT_LOAD_ERROR " + error).c_str()); return 1;
    }
    if (opt.no_js) port::stop_scripting(); // Explicit benchmark baseline only.
#ifndef __vita__
    SDL_StartTextInput();
#else
    // Vita SDL starts a native IME dialog here, covering normal gameplay.
    // Use upstream's controller/front-touch keyboard instead.
    SDL_StopTextInput();
#endif
    game->OnSize(kWidth, kHeight, 8, 8);
    port::log_stage("scene.sized");
    Controller controller(nullptr, SDL_GameControllerClose);
    auto open_controller = [&]() {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (SDL_IsGameController(i)) {
                controller.reset(SDL_GameControllerOpen(i));
                if (controller) { game->OnPadMount(true); std::printf("CONTROLLER_READY %s\n", SDL_GameControllerName(controller.get())); break; }
            }
        }
    };
    open_controller();
    port::log_stage("controller.ready");
    std::vector<AnsiCell> cells(kColumns * kRows);
    std::vector<uint32_t> pixels(kWidth * kHeight);
    // Rolling bounded profiling storage; safe for arbitrarily long play sessions.
    std::vector<double> scene_ms, script_ms, conversion_ms, upload_ms, present_ms, total_ms;
    for (auto* samples : {&scene_ms, &script_ms, &conversion_ms, &upload_ms, &present_ms, &total_ms}) samples->reserve(3600);
    auto record = [](std::vector<double>& samples, double value, int frame) {
        if (samples.size() < 3600) samples.push_back(value);
        else samples[static_cast<std::size_t>(frame) % 3600] = value;
    };
    bool running = true, focused = true;
    int frame = 0;
    uint64_t synthetic = game->stamp;
    float initial_pos[3] = {};
    float initial_yaw = 0;
    bool smoke_baseline = false, moved = false, rotated = false, grounded = false, touch_verified = false;
    const uint64_t run_start = now_us();
    uint64_t next_memory_log = run_start;
    // Engine contact zero is mouse, touch contacts are strictly 1..3.
    std::array<SDL_FingerID, 3> fingers{{-1,-1,-1}};
    while (running && !port::should_exit() && (!opt.frames || frame < opt.frames) &&
           (!opt.seconds || now_us() - run_start < static_cast<uint64_t>(opt.seconds) * 1000000)) {
        const uint64_t begin = now_us();
        // Feed scripted checks through SDL's event queue and the real input path.
        if (opt.smoke) {
            if (opt.verify_gameplay && (frame == 170 || frame == 171)) {
                SDL_Event touch{}; touch.type = frame == 170 ? SDL_FINGERDOWN : SDL_FINGERUP;
                touch.tfinger.touchId=1; touch.tfinger.fingerId=123; touch.tfinger.x=0.2F; touch.tfinger.y=0.5F;
                SDL_PushEvent(&touch);
            }
            if (opt.verify_gameplay && frame == 200) {
                if (!port::eval_script("this.__portResetProbe=123",false,error)) return 1;
                port::request_new_game();
            }
            if (frame == 60) {
                std::copy(game->player.pos, game->player.pos + 3, initial_pos);
                initial_yaw = game->prev_yaw;
                smoke_baseline = true;
            }
            if (frame == 60 || frame == 160) {
                SDL_Event key{};
                key.type = frame == 60 ? SDL_KEYDOWN : SDL_KEYUP;
                key.key.keysym.sym = SDLK_w;
                SDL_PushEvent(&key);
            }
            if (frame == 100 || frame == 160) {
                SDL_Event axis{};
                axis.type = SDL_CONTROLLERAXISMOTION;
                axis.caxis.axis = SDL_CONTROLLER_AXIS_RIGHTX;
                axis.caxis.value = frame == 100 ? -24000 : 0;
                SDL_PushEvent(&axis);
            }
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q && (event.key.keysym.mod & KMOD_CTRL)) running = false;
                if (event.key.keysym.sym == SDLK_F1) {
                    if (event.type == SDL_KEYDOWN && !event.key.repeat) game->ToggleMenu(0);
                    continue; // Upstream's historical F1 camera binding must not leak through.
                }
                // SDL text events supply printable input; Enter is non-printable
                // and upstream deliberately consumes it as KEYB_CHAR.
                if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.sym == SDLK_RETURN)
                    game->OnKeyb(KEYB_CHAR, '\n');
                const int key = port::engine_key(event.key.keysym.sym);
                if (key != A3D_NONE && !event.key.repeat)
                    game->OnKeyb(event.type == SDL_KEYDOWN ? KEYB_DOWN : KEYB_UP, key);
            }
            if (event.type == SDL_TEXTINPUT) {
                const auto* text = reinterpret_cast<const unsigned char*>(event.text.text);
                while (*text) {
                    uint32_t codepoint = *text++;
                    if (codepoint >= 0xC0 && codepoint < 0xF8) {
                        int continuation = codepoint < 0xE0 ? 1 : (codepoint < 0xF0 ? 2 : 3);
                        codepoint &= (1U << (6 - continuation)) - 1;
                        while (continuation-- && (*text & 0xC0) == 0x80) codepoint = (codepoint << 6) | (*text++ & 63);
                    }
                    if (codepoint <= 0x10FFFF) game->OnKeyb(KEYB_CHAR, static_cast<int>(codepoint));
                }
            }
            if (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERUP || event.type == SDL_FINGERMOTION) {
#ifdef __vita__
                if (event.tfinger.touchId != 1) continue; // SDL Vita uses port+1: front=1, rear=2.
#endif
                auto slot = std::find(fingers.begin(), fingers.end(), event.tfinger.fingerId);
                if (event.type == SDL_FINGERDOWN && slot == fingers.end()) slot = std::find(fingers.begin(), fingers.end(), -1);
                if (slot != fingers.end()) {
                    *slot = event.tfinger.fingerId;
                    int window_width = 0, window_height = 0;
                    SDL_GetWindowSize(window.get(), &window_width, &window_height);
                    float x = 0, y = 0;
                    SDL_RenderWindowToLogical(renderer.get(), static_cast<int>(event.tfinger.x * window_width), static_cast<int>(event.tfinger.y * window_height), &x, &y);
                    game->OnTouch(event.type == SDL_FINGERDOWN ? TOUCH_BEGIN : (event.type == SDL_FINGERUP ? TOUCH_END : TOUCH_MOVE),
                                  port::engine_touch_contact(static_cast<int>(slot - fingers.begin())), static_cast<int>(x), static_cast<int>(y));
                    if (opt.verify_gameplay && event.type == SDL_FINGERDOWN && event.tfinger.fingerId == 123)
                        touch_verified = game->input.contact[1].action != Game::Input::Contact::NONE;
                    if (event.type == SDL_FINGERUP) *slot = -1;
                }
            }
            if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP || event.type == SDL_MOUSEMOTION) {
                int x = event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x;
                int y = event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y;
                float logical_x = 0, logical_y = 0;
                SDL_RenderWindowToLogical(renderer.get(), x, y, &logical_x, &logical_y);
                GAME_MOUSE action = MOUSE_MOVE;
                if (event.type != SDL_MOUSEMOTION) {
                    const bool down = event.type == SDL_MOUSEBUTTONDOWN;
                    if (event.button.button == SDL_BUTTON_LEFT) action = down ? MOUSE_LEFT_BUT_DOWN : MOUSE_LEFT_BUT_UP;
                    else if (event.button.button == SDL_BUTTON_RIGHT) action = down ? MOUSE_RIGHT_BUT_DOWN : MOUSE_RIGHT_BUT_UP;
                }
                game->OnMouse(action, static_cast<int>(logical_x), static_cast<int>(logical_y));
            }
            if (event.type == SDL_CONTROLLERDEVICEADDED && !controller) open_controller();
            if (event.type == SDL_MOUSEWHEEL) {
                int x = 0, y = 0; SDL_GetMouseState(&x, &y);
                float lx = 0, ly = 0; SDL_RenderWindowToLogical(renderer.get(), x, y, &lx, &ly);
                int direction = event.wheel.y;
                if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) direction = -direction;
                if (direction) game->OnMouse(direction > 0 ? MOUSE_WHEEL_UP : MOUSE_WHEEL_DOWN, static_cast<int>(lx), static_cast<int>(ly));
            }
            if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller &&
                event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller.get()))) {
                game->OnPadMount(false); controller.reset(); open_controller();
            }
            if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) {
                const bool down = event.type == SDL_CONTROLLERBUTTONDOWN;
                // Select+Start opens upstream's controller-driven text keyboard.
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START && down && controller &&
                    SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_BACK)) {
                    game->CloseMenu(); game->OnPadButton(10, true); game->OnPadButton(10, false); continue;
                }
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
                    game->OnKeyb(down ? KEYB_DOWN : KEYB_UP, A3D_Q);
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
                    game->OnKeyb(down ? KEYB_DOWN : KEYB_UP, A3D_E);
                const int button = port::engine_button(event.cbutton.button);
                if (button >= 0) game->OnPadButton(button, down);
                if (!down || (game->menu_depth < 0 && !game->show_inventory)) {
                    int key = A3D_NONE;
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) key = A3D_UP;
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) key = A3D_DOWN;
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) key = A3D_LEFT;
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) key = A3D_RIGHT;
                    if (key != A3D_NONE) game->OnKeyb(down ? KEYB_DOWN : KEYB_UP, key);
                }
            }
            if (event.type == SDL_CONTROLLERAXISMOTION) {
                const int axis = event.caxis.axis;
                if (axis < 2) game->OnPadAxis(axis, port::axis_value(event.caxis.value));
                if (axis == SDL_CONTROLLER_AXIS_RIGHTX) {
                    const int16_t value = port::axis_value(event.caxis.value);
                    game->OnPadAxis(4, value < 0 ? static_cast<int16_t>(std::min(32767, -static_cast<int>(value))) : 0);
                    game->OnPadAxis(5, value > 0 ? value : 0);
                }
            }
            if (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST || event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)) {
                focused = event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED;
                if (!focused) fingers.fill(-1);
                game->OnFocus(focused);
                port::pause_audio(!focused);
                std::printf("LIFECYCLE focus=%d\n", focused);
            }
            if (event.type == SDL_APP_WILLENTERBACKGROUND || event.type == SDL_APP_DIDENTERFOREGROUND) {
                focused = event.type == SDL_APP_DIDENTERFOREGROUND;
                if (!focused) fingers.fill(-1);
                game->OnFocus(focused);
                port::pause_audio(!focused);
                std::printf("LIFECYCLE background=%d\n", !focused);
            }
        }
        if (!running) break;
        if (port::take_new_game_request()) {
            port::close_scene();
            if (!port::load_scene(opt.assets, opt.smoke ? 1000000 : now_us(), error)) { port::log_stage(("NEW_GAME_ERROR " + error).c_str()); return 1; }
            if (opt.verify_gameplay && !port::eval_script("if(this.__portResetProbe!==undefined)throw Error('stale JS state after New Game')",false,error)) {
                port::log_stage(("NEW_GAME_ERROR " + error).c_str()); return 1;
            }
            game->OnSize(kWidth, kHeight, 8, 8);
            game->OnPadMount(controller != nullptr);
            InitAudio(); AudioMute(game->mute);
            if (opt.verify_gameplay) AudioMute(false);
            if (!opt.script.empty() && !port::load_script(opt.script, error)) { port::log_stage(("SCRIPT_LOAD_ERROR " + error).c_str()); return 1; }
            if (opt.no_js) port::stop_scripting();
            synthetic = game->stamp;
            fingers.fill(-1);
            port::log_stage("NEW_GAME_OK runtime/world/inventory reset");
        }
        if (!focused && !opt.smoke) { SDL_Delay(20); continue; }
        if (opt.smoke) synthetic += 16667;
        const uint64_t scene_start = now_us();
        const uint64_t script_start = port::script_time_us();
        if (frame == 0) port::log_stage("render.begin");
        game->Render(opt.smoke ? synthetic : now_us(), cells.data(), kColumns, kRows);
        if (frame == 0) port::log_stage("render.ready");
        const uint64_t scene_end = now_us();
        record(script_ms, (port::script_time_us() - script_start) / 1000.0, frame);
        constexpr const char* hints = "MOVE WASD/STICK | CAMERA Q E/L R | JUMP SPACE/O | ATTACK ENTER/X | MENU F1/SELECT | INVENTORY I/START | CHAT TAB";
        const char* status = opt.no_js ? "ASCIICKER VITA | JS DISABLED FOR BENCHMARK | OFFLINE" :
            (port::audio_ready() ? "ASCIICKER VITA | QUICKJS 2026-06-04 + SDL AUDIO | OFFLINE" : "ASCIICKER VITA | QUICKJS 2026-06-04 | AUDIO UNAVAILABLE");
        for (int row = 0; row < 2; ++row) {
            const char* text = row == 0 ? status : hints;
            const auto len = std::strlen(text);
            for (int col = 0; col < kColumns; ++col)
                cells[row * kColumns + col] = {231, 16, static_cast<uint8_t>(static_cast<std::size_t>(col) < len ? text[col] : ' '), 0};
        }
        if (smoke_baseline) {
            moved = moved || std::hypot(game->player.pos[0] - initial_pos[0], game->player.pos[1] - initial_pos[1]) > 0.5F;
            rotated = rotated || std::fabs(game->prev_yaw - initial_yaw) > 1.0F;
            grounded = grounded || game->prev_grounded > 0;
        }
        if (!port::rasterize(cells.data(), kColumns, kRows, font, pixels.data(), kWidth * 4, pixels.size() * 4)) {
            std::fprintf(stderr, "RASTER_ERROR\n"); return 1;
        }
        const uint64_t conversion_end = now_us();
        void* texture_pixels = nullptr;
        int texture_pitch = 0;
        if (SDL_LockTexture(texture.get(), nullptr, &texture_pixels, &texture_pitch) != 0) {
            std::fprintf(stderr, "TEXTURE_LOCK_ERROR %s\n", SDL_GetError()); return 1;
        }
        if (!texture_pixels || texture_pitch < kWidth * 4) {
            SDL_UnlockTexture(texture.get());
            std::fprintf(stderr, "TEXTURE_PITCH_ERROR pitch=%d\n", texture_pitch); return 1;
        }
        // Streaming textures may have padded rows; never assume contiguous storage.
        for (int row = 0; row < kHeight; ++row)
            std::memcpy(static_cast<uint8_t*>(texture_pixels) + row * texture_pitch,
                        pixels.data() + row * kWidth, kWidth * sizeof(uint32_t));
        SDL_UnlockTexture(texture.get());
        if (SDL_RenderClear(renderer.get()) != 0 || SDL_RenderCopy(renderer.get(), texture.get(), nullptr, nullptr) != 0) {
            std::fprintf(stderr, "PRESENT_ERROR %s\n", SDL_GetError()); return 1;
        }
        const uint64_t upload_end = now_us();
        SDL_RenderPresent(renderer.get());
        if (frame == 0) port::log_stage("present.ready");
        const uint64_t end = now_us();
        record(scene_ms, (scene_end - scene_start) / 1000.0, frame);
        record(conversion_ms, (conversion_end - scene_end) / 1000.0, frame);
        record(upload_ms, (upload_end - conversion_end) / 1000.0, frame);
        record(present_ms, (end - upload_end) / 1000.0, frame);
        record(total_ms, (end - begin) / 1000.0, frame);
        ++frame;
        if (end >= next_memory_log) {
            std::printf("MEMORY elapsed_s=%.1f rss_kib=%ld\n", (end - run_start) / 1000000.0, resident_kib());
            next_memory_log = end + 60000000;
        }
        if (frame == 1 || frame % 600 == 0) {
            std::printf("FRAME_OK frame=%d pos=%.2f,%.2f,%.2f yaw=%.2f total_ms=%.2f\n", frame,
                game->player.pos[0], game->player.pos[1], game->player.pos[2], game->prev_yaw, total_ms.back());
            char progress[160];
            std::snprintf(progress, sizeof(progress), "FRAME_PROGRESS frame=%d pos=%.2f,%.2f,%.2f yaw=%.2f", frame,
                          game->player.pos[0], game->player.pos[1], game->player.pos[2], game->prev_yaw);
            port::log_stage(progress);
        }
    }
    if (opt.verify_gameplay) {
        try {
            if (!touch_verified) throw std::runtime_error("first SDL touch did not reach engine contact 1");
            gameplay_checks(opt.assets);
            if (!port::audio_ready()) throw std::runtime_error("SDL playback device unavailable");
            if (!port::audio_callback_count() || !port::audio_signal_count()) throw std::runtime_error("SDL audio callback produced no signal");
            port::pause_audio(true);
            const auto paused_callbacks = port::audio_callback_count();
            SDL_Delay(60);
            if (port::audio_callback_count() != paused_callbacks) throw std::runtime_error("audio did not pause");
            port::pause_audio(false); SDL_Delay(100);
            if (port::audio_callback_count() == paused_callbacks) throw std::runtime_error("audio did not resume");
            port::log_stage("GAMEPLAY_CHECKS_OK scripts callbacks limits inventory menus audio");
        } catch (const std::exception& exception) {
            port::log_stage((std::string("GAMEPLAY_CHECKS_FAIL ") + exception.what()).c_str()); return 1;
        }
    }
    if (!opt.screenshot.empty()) {
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface(SDL_CreateRGBSurfaceWithFormatFrom(
            pixels.data(), kWidth, kHeight, 32, kWidth * 4, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
        if (!surface || SDL_SaveBMP(surface.get(), opt.screenshot.c_str()) != 0) {
            std::fprintf(stderr, "SCREENSHOT_ERROR %s\n", SDL_GetError()); return 1;
        }
        std::printf("SCREENSHOT_OK %s\n", opt.screenshot.c_str());
    }
    char report[512];
    const auto js_stats = port::script_stats();
    std::snprintf(report, sizeof(report), "SCRIPT_PROFILE calls=%llu errors=%llu total_us=%llu memory_bytes=%lld audio_dropped=%u audio_callbacks=%u audio_signal=%u",
                  static_cast<unsigned long long>(js_stats.calls), static_cast<unsigned long long>(js_stats.errors),
                  static_cast<unsigned long long>(js_stats.microseconds), static_cast<long long>(js_stats.memory_bytes), port::audio_dropped(), port::audio_callback_count(), port::audio_signal_count());
    port::log_stage(report);
    std::snprintf(report, sizeof(report), "PROFILE frames=%d total_p50=%.2f total_p95=%.2f total_max=%.2f scene_p50=%.2f glyph_p50=%.2f upload_p50=%.2f present_p50=%.2f js_p50=%.3f js_p95=%.3f ms",
        frame, percentile(total_ms, .5), percentile(total_ms, .95), percentile(total_ms, 1),
        percentile(scene_ms, .5), percentile(conversion_ms, .5), percentile(upload_ms, .5), percentile(present_ms, .5), percentile(script_ms, .5), percentile(script_ms, .95));
    port::log_stage(report);
    if (opt.smoke) {
        const bool complete = opt.seconds ? now_us() - run_start >= static_cast<uint64_t>(opt.seconds) * 1000000 : frame >= opt.frames;
        std::snprintf(report, sizeof(report), "SMOKE_%s frames=%d movement=%d camera=%d grounded=%d elapsed_s=%.1f", complete && moved && rotated && grounded ? "OK" : "FAIL", frame, moved, rotated, grounded, (now_us() - run_start) / 1000000.0);
        port::log_stage(report);
        if (!complete || !moved || !rotated || !grounded) return 1;
    }
    controller.reset();
    port::close_scene();
    port::log_stage("SHUTDOWN_OK scene/resources released");
    return 0;
}
