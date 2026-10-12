#include <SDL2/SDL.h>
#include "games/asciicker/scene.h"
#include "games/asciicker/world_fixture.h"
#include "games/asciicker/gpu_scene.h"
#include "games/asciicker/ascii_resources.h"
#include "platform/graphics/device.h"
#include "engine/render/camera.h"
#include "app/self_test.h"
#include "app/graphics_fixture.h"
#include "platform/engine_hooks.h"
#include "platform/input.h"
#include "platform/native_touch.h"
#include "platform/boot_screen.h"
#include "platform/shader_probe.h"
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
extern Terrain *terrain;
extern World *world;
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
    bool probe_gpu = false;
    bool gpu_fixture = false;
    bool no_js = false;
    bool verify_gameplay = false;
    bool unpaced = false;
    std::string script;
};
bool options(int argc, char **argv, Options &out) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--smoke")
            out.smoke = true;
        else if (arg == "--gpu-scene")
            continue; // Compatibility: GPU is now the sole renderer.
        else if (arg == "--no-js")
            out.no_js = true;
        else if (arg == "--unpaced")
            out.unpaced = true;
        else if (arg == "--verify-gameplay")
            out.verify_gameplay = true;
        else if (arg == "--probe-js")
            out.probe = true;
        else if (arg == "--probe-gpu")
            out.probe_gpu = true;
        else if (arg == "--gpu-fixture")
            out.gpu_fixture = true;
        else if ((arg == "--script" || arg == "--assets" || arg == "--manifest" || arg == "--frames" ||
                  arg == "--seconds" || arg == "--screenshot") &&
                 i + 1 < argc) {
            const char *value = argv[++i];
            if (arg == "--script")
                out.script = value;
            if (arg == "--assets")
                out.assets = value;
            if (arg == "--manifest")
                out.manifest = value;
            if (arg == "--screenshot")
                out.screenshot = value;
            if (arg == "--frames" || arg == "--seconds") {
                char *end = nullptr;
                const long count = std::strtol(value, &end, 10);
                if (*end || count < 1 || count > 1000000)
                    return false;
                if (arg == "--frames")
                    out.frames = static_cast<int>(count);
                else
                    out.seconds = static_cast<int>(count);
            }
        } else
            return false;
    }
    const int diagnostics =
        static_cast<int>(out.probe) + static_cast<int>(out.probe_gpu) + static_cast<int>(out.gpu_fixture);
    return diagnostics <= 1 &&
           (!out.unpaced || (diagnostics == 0 && !out.verify_gameplay && (out.frames || out.seconds))) &&
           (!out.gpu_fixture || (!out.smoke && !out.verify_gameplay && out.seconds == 0)) &&
           (!out.smoke || out.frames >= 180 || out.seconds > 0) &&
           (!out.verify_gameplay || (out.smoke && !out.no_js)) && (!out.no_js || out.script.empty());
}
struct SdlLifetime {
    ~SdlLifetime() {
        SDL_Quit();
    }
};
struct SceneLifetime {
    ~SceneLifetime() {
        port::close_scene();
    }
};
double percentile(std::vector<double> values, double fraction) {
    if (values.empty())
        return 0;
    std::sort(values.begin(), values.end());
    return values[static_cast<std::size_t>((values.size() - 1) * fraction)];
}
long resident_kib() {
#ifndef __vita__
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        long value = 0;
        if (std::sscanf(line.c_str(), "VmRSS: %ld kB", &value) == 1)
            return value;
    }
#endif
    return -1;
}
} // namespace

int main(int argc, char **argv) {
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
        std::fprintf(stderr,
                     "Usage: vita_sdl2 [--assets DIR] [--manifest FILE] [--frames N] [--seconds N] [--smoke "
                     "--frames 180+] [--verify-gameplay] [--script FILE | --no-js] [--screenshot FILE.bmp] "
                     "[--unpaced (finite benchmark only)] [--probe-js | --probe-gpu | --gpu-fixture]\nGPU "
                     "rendering is required. GPU fixture: "
                     "omit --assets for depth/presentation tests; provide --assets for real terrain.\n");
        return 2;
    }
    std::string error;
    if (opt.gpu_fixture) {
        const int frames = opt.frames ? opt.frames : 120;
        const bool success =
            opt.assets.empty()
                ? port::graphics::run_geometry_fixture(frames, opt.screenshot, error)
                : port::asciicker::run_world_fixture(opt.assets, frames, opt.screenshot, error);
        if (!success) {
            port::log_stage(("GPU_FIXTURE_ERROR " + error).c_str());
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        return 0;
    }
    if (opt.probe_gpu) {
        if (!port::probe_vita_shader_compiler(error)) {
            port::log_stage(("GPU_SHADER_PROBE_ERROR " + error).c_str());
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        return 0;
    }
    if (opt.probe) {
        if (!port::probe_quickjs(error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        std::puts("QUICKJS_PROBE_OK eval native-call shared-buffer exception teardown");
        return 0;
    }
    std::puts("SDL_INIT_BEGIN video controller timer");
    port::BootScreen boot_screen;
    uint32_t sdl_flags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER;
#ifdef __vita__
    sdl_flags &= ~SDL_INIT_VIDEO;
#endif
    if (SDL_Init(sdl_flags) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SdlLifetime sdl_lifetime;
    port::NativeTouch native_touch;
#ifdef __vita__
    if (!native_touch.initialize())
        port::log_stage("TOUCH_UNAVAILABLE native front panel");
#endif
    std::puts("SDL_INIT_OK");
    port::log_stage("sdl.ready");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
#ifdef __vita__
    if (opt.assets.empty())
        opt.assets = "app0:assets";
    if (opt.manifest.empty())
        opt.manifest = "app0:assets/manifest.txt";
    port::set_config_path("ux0:data/asciicker/asciicker.cfg");
#else
    std::unique_ptr<char, decltype(&SDL_free)> executable_dir(SDL_GetBasePath(), SDL_free);
    if (opt.assets.empty())
        opt.assets = executable_dir ? std::string(executable_dir.get()) + "../../assets" : "assets";
    if (opt.manifest.empty())
        opt.manifest = executable_dir ? std::string(executable_dir.get()) + "asset_manifest.txt"
                                      : "build/linux/asset_manifest.txt";
    std::unique_ptr<char, decltype(&SDL_free)> prefs(SDL_GetPrefPath("asciicker", "vita-port"), SDL_free);
    if (prefs)
        port::set_config_path((std::string(prefs.get()) + "asciicker.cfg").c_str());
#endif
    boot_screen.progress(10);
    if (!port::validate_assets(opt.assets, opt.manifest, error)) {
        std::fprintf(stderr, "ASSET_ERROR %s\n", error.c_str());
        return 1;
    }
    using Controller = std::unique_ptr<SDL_GameController, decltype(&SDL_GameControllerClose)>;
    std::unique_ptr<port::graphics::DeviceFactory> graphics_factory;
    std::unique_ptr<port::graphics::Device> graphics_device;
    boot_screen.progress(20);
    graphics_factory = port::graphics::create_device_factory(error);
    if (graphics_factory)
        graphics_device = graphics_factory->create(kColumns * 2 + 4, kRows * 2 + 4, error);
    if (!graphics_device) {
        port::log_stage(("GPU_SCENE_INIT_ERROR " + error).c_str());
        return 1;
    }
    port::log_stage("PRESENTER gpu-ascii scene=gpu");
    graphics_device->set_display_pacing(!opt.unpaced);
    port::log_stage(opt.unpaced ? "PACING unpaced-diagnostic" : "PACING default");
    const char *video_driver = SDL_GetCurrentVideoDriver();
    std::printf("SDL_READY driver=%s renderer=%s grid=%dx%d pixels=%dx%d\n",
                video_driver ? video_driver : "native", graphics_device->name(), kColumns, kRows, kWidth,
                kHeight);
    MakeStamp = now_us;
    port::reset_exit();
    boot_screen.progress(45);
    if (!port::load_scene(opt.assets, opt.smoke ? 1000000 : now_us(), error)) {
        std::fprintf(stderr, "SCENE_ERROR %s\n", error.c_str());
        return 1;
    }
    SceneLifetime scene_lifetime;
    boot_screen.progress(65);
    std::unique_ptr<engine::SceneBackend> gpu_scene;
    {
        engine::AsciiResources ascii;
        if (!port::asciicker::collect_ascii_resources(opt.assets + "/fonts/cp437_8x8.png", ascii, error) ||
            !graphics_device->set_ascii(ascii, error)) {
            port::log_stage(("GPU_ASCII_ERROR " + error).c_str());
            return 1;
        }
        gpu_scene = port::asciicker::create_gpu_scene(*graphics_device, terrain, world, error);
        if (!gpu_scene) {
            port::log_stage(("GPU_SCENE_ERROR " + error).c_str());
            return 1;
        }
        SetSceneBackend(game->renderer, gpu_scene.get());
    }
    boot_screen.progress(85);
    InitAudio(); // Device failure is visible and degrades to silent gameplay.
    AudioMute(game->mute);
    if (opt.verify_gameplay)
        AudioMute(false);
    if (!opt.script.empty() && !port::load_script(opt.script, error)) {
        port::log_stage(("SCRIPT_LOAD_ERROR " + error).c_str());
        return 1;
    }
    if (opt.no_js)
        port::stop_scripting(); // Explicit benchmark baseline only.
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
                if (controller) {
                    game->OnPadMount(true);
                    std::printf("CONTROLLER_READY %s\n", SDL_GameControllerName(controller.get()));
                    break;
                }
            }
        }
    };
    open_controller();
    auto input_size = [&](int &width, int &height) {
        width = kWidth;
        height = kHeight;
        SDL_Window *input_window = SDL_GetKeyboardFocus();
        if (input_window)
            SDL_GetWindowSize(input_window, &width, &height);
    };
    auto logical_input = [&](int x, int y, float &lx, float &ly) {
        int width, height;
        input_size(width, height);
        const auto viewport = engine::fit_viewport({width, height});
        lx = (x - viewport.x) * static_cast<float>(kWidth) / std::max(1, viewport.width);
        ly = (y - viewport.y) * static_cast<float>(kHeight) / std::max(1, viewport.height);
        const bool inside = lx >= 0 && ly >= 0 && lx <= kWidth && ly <= kHeight;
        lx = std::clamp(lx, 0.0F, static_cast<float>(kWidth - 1));
        ly = std::clamp(ly, 0.0F, static_cast<float>(kHeight - 1));
        return inside;
    };
    port::log_stage("controller.ready");
    boot_screen.progress(95);
    std::vector<AnsiCell> cells(kColumns * kRows);
    // Rolling bounded profiling storage; safe for arbitrarily long play sessions.
    std::vector<double> scene_ms, script_ms, conversion_ms, upload_ms, present_ms, total_ms;
    std::vector<double> collect_ms, reflection_ms, primary_ms, game_ui_ms;
    for (auto *samples : {&collect_ms, &reflection_ms, &primary_ms, &game_ui_ms})
        samples->reserve(3600);
    for (auto *samples : {&scene_ms, &script_ms, &conversion_ms, &upload_ms, &present_ms, &total_ms})
        samples->reserve(3600);
    auto record = [](std::vector<double> &samples, double value, int frame) {
        if (samples.size() < 3600)
            samples.push_back(value);
        else
            samples[static_cast<std::size_t>(frame) % 3600] = value;
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
    std::array<SDL_FingerID, 3> fingers{{-1, -1, -1}};
    while (running && !port::should_exit() && (!opt.frames || frame < opt.frames) &&
           (!opt.seconds || now_us() - run_start < static_cast<uint64_t>(opt.seconds) * 1000000)) {
        const uint64_t begin = now_us();
        // Feed scripted checks through SDL's event queue and the real input path.
        if (opt.smoke) {
            if (opt.verify_gameplay && (frame == 170 || frame == 171)) {
                SDL_Event touch{};
                touch.type = frame == 170 ? SDL_FINGERDOWN : SDL_FINGERUP;
                touch.tfinger.touchId = 1;
                touch.tfinger.fingerId = 123;
                int input_width, input_height;
                input_size(input_width, input_height);
                const auto viewport = engine::fit_viewport({input_width, input_height});
                touch.tfinger.x = (viewport.x + viewport.width * 0.2F) / std::max(1, input_width);
                touch.tfinger.y = (viewport.y + viewport.height * 0.5F) / std::max(1, input_height);
                SDL_PushEvent(&touch);
            }
            if (opt.verify_gameplay && frame == 200) {
                if (!port::eval_script("this.__portResetProbe=123", false, error))
                    return 1;
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
        native_touch.poll();
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q &&
                    (event.key.keysym.mod & KMOD_CTRL))
                    running = false;
                if (event.key.keysym.sym == SDLK_F1) {
                    if (event.type == SDL_KEYDOWN && !event.key.repeat)
                        game->ToggleMenu(0);
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
                const auto *text = reinterpret_cast<const unsigned char *>(event.text.text);
                while (*text) {
                    uint32_t codepoint = *text++;
                    if (codepoint >= 0xC0 && codepoint < 0xF8) {
                        int continuation = codepoint < 0xE0 ? 1 : (codepoint < 0xF0 ? 2 : 3);
                        codepoint &= (1U << (6 - continuation)) - 1;
                        while (continuation-- && (*text & 0xC0) == 0x80)
                            codepoint = (codepoint << 6) | (*text++ & 63);
                    }
                    if (codepoint <= 0x10FFFF)
                        game->OnKeyb(KEYB_CHAR, static_cast<int>(codepoint));
                }
            }
            if (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERUP ||
                event.type == SDL_FINGERMOTION) {
#ifdef __vita__
                if (event.tfinger.touchId != 1)
                    continue; // SDL Vita uses port+1: front=1, rear=2.
#endif
                auto slot = std::find(fingers.begin(), fingers.end(), event.tfinger.fingerId);
                if (event.type == SDL_FINGERDOWN && slot == fingers.end())
                    slot = std::find(fingers.begin(), fingers.end(), -1);
                if (slot != fingers.end()) {
                    *slot = event.tfinger.fingerId;
                    int window_width = 0, window_height = 0;
                    input_size(window_width, window_height);
                    float x = 0, y = 0;
                    const bool inside =
                        logical_input(static_cast<int>(event.tfinger.x * window_width),
                                      static_cast<int>(event.tfinger.y * window_height), x, y);
                    if (event.type == SDL_FINGERDOWN && !inside) {
                        *slot = -1;
                        continue;
                    }
                    game->OnTouch(event.type == SDL_FINGERDOWN
                                      ? TOUCH_BEGIN
                                      : (event.type == SDL_FINGERUP ? TOUCH_END : TOUCH_MOVE),
                                  port::engine_touch_contact(static_cast<int>(slot - fingers.begin())),
                                  static_cast<int>(x), static_cast<int>(y));
                    if (opt.verify_gameplay && event.type == SDL_FINGERDOWN && event.tfinger.fingerId == 123)
                        touch_verified = game->input.contact[1].action != Game::Input::Contact::NONE;
                    if (event.type == SDL_FINGERUP)
                        *slot = -1;
                }
            }
            if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP ||
                event.type == SDL_MOUSEMOTION) {
                int x = event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x;
                int y = event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y;
                float logical_x = 0, logical_y = 0;
                const bool inside = logical_input(x, y, logical_x, logical_y);
                if (event.type == SDL_MOUSEBUTTONDOWN && !inside)
                    continue;
                GAME_MOUSE action = MOUSE_MOVE;
                if (event.type != SDL_MOUSEMOTION) {
                    const bool down = event.type == SDL_MOUSEBUTTONDOWN;
                    if (event.button.button == SDL_BUTTON_LEFT)
                        action = down ? MOUSE_LEFT_BUT_DOWN : MOUSE_LEFT_BUT_UP;
                    else if (event.button.button == SDL_BUTTON_RIGHT)
                        action = down ? MOUSE_RIGHT_BUT_DOWN : MOUSE_RIGHT_BUT_UP;
                }
                game->OnMouse(action, static_cast<int>(logical_x), static_cast<int>(logical_y));
            }
            if (event.type == SDL_CONTROLLERDEVICEADDED && !controller)
                open_controller();
            if (event.type == SDL_MOUSEWHEEL) {
                int x = 0, y = 0;
                SDL_GetMouseState(&x, &y);
                float lx = 0, ly = 0;
                if (!logical_input(x, y, lx, ly))
                    continue;
                int direction = event.wheel.y;
                if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
                    direction = -direction;
                if (direction)
                    game->OnMouse(direction > 0 ? MOUSE_WHEEL_UP : MOUSE_WHEEL_DOWN, static_cast<int>(lx),
                                  static_cast<int>(ly));
            }
            if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller &&
                event.cdevice.which ==
                    SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller.get()))) {
                game->OnPadMount(false);
                controller.reset();
                open_controller();
            }
            if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) {
                const bool down = event.type == SDL_CONTROLLERBUTTONDOWN;
                // Select+Start opens upstream's controller-driven text keyboard.
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START && down && controller &&
                    SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_BACK)) {
                    game->CloseMenu();
                    game->OnPadButton(10, true);
                    game->OnPadButton(10, false);
                    continue;
                }
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
                    game->OnKeyb(down ? KEYB_DOWN : KEYB_UP, A3D_Q);
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
                    game->OnKeyb(down ? KEYB_DOWN : KEYB_UP, A3D_E);
                const int button = port::engine_button(event.cbutton.button);
                if (button >= 0)
                    game->OnPadButton(button, down);
                if (!down || (game->menu_depth < 0 && !game->show_inventory)) {
                    int key = A3D_NONE;
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP)
                        key = A3D_UP;
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
                        key = A3D_DOWN;
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
                        key = A3D_LEFT;
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
                        key = A3D_RIGHT;
                    if (key != A3D_NONE)
                        game->OnKeyb(down ? KEYB_DOWN : KEYB_UP, key);
                }
            }
            if (event.type == SDL_CONTROLLERAXISMOTION) {
                const int axis = event.caxis.axis;
                if (axis < 2)
                    game->OnPadAxis(axis, port::axis_value(event.caxis.value));
                if (axis == SDL_CONTROLLER_AXIS_RIGHTX) {
                    const int16_t value = port::axis_value(event.caxis.value);
                    game->OnPadAxis(
                        4, value < 0 ? static_cast<int16_t>(std::min(32767, -static_cast<int>(value))) : 0);
                    game->OnPadAxis(5, value > 0 ? value : 0);
                }
            }
            if (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                                                  event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)) {
                focused = event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED;
                if (!focused) {
                    fingers.fill(-1);
                    native_touch.reset();
                }
                game->OnFocus(focused);
                port::pause_audio(!focused);
                std::printf("LIFECYCLE focus=%d\n", focused);
            }
            if (event.type == SDL_APP_WILLENTERBACKGROUND || event.type == SDL_APP_DIDENTERFOREGROUND) {
                focused = event.type == SDL_APP_DIDENTERFOREGROUND;
                if (!focused) {
                    fingers.fill(-1);
                    native_touch.reset();
                }
                game->OnFocus(focused);
                port::pause_audio(!focused);
                std::printf("LIFECYCLE background=%d\n", !focused);
            }
        }
        if (!running)
            break;
        if (port::take_new_game_request()) {
            if (gpu_scene)
                SetSceneBackend(game->renderer, nullptr);
            gpu_scene.reset();
            port::close_scene();
            if (!port::load_scene(opt.assets, opt.smoke ? 1000000 : now_us(), error)) {
                port::log_stage(("NEW_GAME_ERROR " + error).c_str());
                return 1;
            }
            if (opt.verify_gameplay &&
                !port::eval_script(
                    "if(this.__portResetProbe!==undefined)throw Error('stale JS state after New Game')",
                    false, error)) {
                port::log_stage(("NEW_GAME_ERROR " + error).c_str());
                return 1;
            }
            game->OnSize(kWidth, kHeight, 8, 8);
            {
                gpu_scene = port::asciicker::create_gpu_scene(*graphics_device, terrain, world, error);
                if (!gpu_scene) {
                    port::log_stage(("GPU_SCENE_ERROR " + error).c_str());
                    return 1;
                }
                SetSceneBackend(game->renderer, gpu_scene.get());
            }
            game->OnPadMount(controller != nullptr);
            InitAudio();
            AudioMute(game->mute);
            if (opt.verify_gameplay)
                AudioMute(false);
            if (!opt.script.empty() && !port::load_script(opt.script, error)) {
                port::log_stage(("SCRIPT_LOAD_ERROR " + error).c_str());
                return 1;
            }
            if (opt.no_js)
                port::stop_scripting();
            synthetic = game->stamp;
            fingers.fill(-1);
            port::log_stage("NEW_GAME_OK runtime/world/inventory reset");
        }
        if (!focused && !opt.smoke) {
            SDL_Delay(20);
            continue;
        }
        if (opt.smoke)
            synthetic += 16667;
        const uint64_t scene_start = now_us();
        const uint64_t script_start = port::script_time_us();
        if (frame == 0)
            port::log_stage("render.begin");
        const bool reload_trace = opt.verify_gameplay && frame >= 200 && frame <= 202;
        if (reload_trace)
            port::log_stage("RELOAD_TRACE scene.begin");
        game->Render(opt.smoke ? synthetic : now_us(), cells.data(), kColumns, kRows);
        if (reload_trace)
            port::log_stage("RELOAD_TRACE scene.end");
        if (const char *backend_error = GetSceneBackendError(game->renderer)) {
            port::log_stage((std::string("SCENE_BACKEND_ERROR ") + backend_error).c_str());
            return 1;
        }
        if (frame == 0)
            port::log_stage("render.ready");
        const uint64_t scene_end = now_us();
        if (gpu_scene) {
            const auto timings = gpu_scene->timings();
            record(collect_ms, timings.collect_ms, frame);
            record(reflection_ms, timings.reflection_ms, frame);
            record(primary_ms, timings.primary_ms, frame);
            // Subtract contiguous backend stages per frame BEFORE computing
            // percentiles. Subtracting stage medians would misattribute stalls.
            // This remainder includes physics/AI/JS and upstream UI generation.
            record(game_ui_ms, std::max(0.0, (scene_end - scene_start) / 1000.0 - timings.collect_ms -
                                                timings.reflection_ms - timings.primary_ms), frame);
        }
        record(script_ms, (port::script_time_us() - script_start) / 1000.0, frame);
        constexpr const char *hints = "MOVE WASD/STICK | CAMERA Q E/L R | JUMP SPACE/O | ATTACK ENTER/X | "
                                      "MENU F1/SELECT | INVENTORY I/START | CHAT TAB";
        const char *status =
            opt.no_js ? "ASCIICKER VITA | JS DISABLED FOR BENCHMARK | OFFLINE"
                      : (port::audio_ready() ? "ASCIICKER VITA | QUICKJS 2026-06-04 + SDL AUDIO | OFFLINE"
                                             : "ASCIICKER VITA | QUICKJS 2026-06-04 | AUDIO UNAVAILABLE");
        for (int row = 0; row < 2; ++row) {
            const char *text = row == 0 ? status : hints;
            const auto len = std::strlen(text);
            for (int col = 0; col < kColumns; ++col)
                cells[row * kColumns + col] = {
                    231, 16, static_cast<uint8_t>(static_cast<std::size_t>(col) < len ? text[col] : ' '), 0};
        }
        if (smoke_baseline) {
            moved = moved || std::hypot(game->player.pos[0] - initial_pos[0],
                                        game->player.pos[1] - initial_pos[1]) > 0.5F;
            rotated = rotated || std::fabs(game->prev_yaw - initial_yaw) > 1.0F;
            grounded = grounded || game->prev_grounded > 0;
        }
        if (!graphics_device->set_overlay(cells.data(), kColumns, kRows, error)) {
            port::log_stage(("GPU_OVERLAY_ERROR " + error).c_str());
            return 1;
        }
        if (reload_trace)
            port::log_stage("RELOAD_TRACE overlay.ready");
        const uint64_t conversion_end = now_us();
        const uint64_t upload_end = now_us();
        if (reload_trace)
            port::log_stage("RELOAD_TRACE present.begin");
        if (!graphics_device->present(error)) {
            port::log_stage(("GPU_PRESENT_ERROR " + error).c_str());
            return 1;
        }
        if (reload_trace)
            port::log_stage("RELOAD_TRACE present.end");
        if (frame == 0) {
            boot_screen.close();
            port::log_stage("present.ready");
        }
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
                        game->player.pos[0], game->player.pos[1], game->player.pos[2], game->prev_yaw,
                        total_ms.back());
            char progress[160];
            std::snprintf(progress, sizeof(progress), "FRAME_PROGRESS frame=%d pos=%.2f,%.2f,%.2f yaw=%.2f",
                          frame, game->player.pos[0], game->player.pos[1], game->player.pos[2],
                          game->prev_yaw);
            port::log_stage(progress);
        }
        if (frame % 120 == 0) {
            char timing[512];
            std::snprintf(timing, sizeof(timing),
                          "LIVE_PROFILE frame=%d total_p50=%.2f scene_p50=%.2f collect_p50=%.2f "
                          "reflection_p50=%.2f primary_p50=%.2f overlay_p50=%.2f present_p50=%.2f "
                          "js_p50=%.3f game_ui_p50=%.2f ms",
                          frame, percentile(total_ms, .5), percentile(scene_ms, .5),
                          percentile(collect_ms, .5), percentile(reflection_ms, .5),
                          percentile(primary_ms, .5), percentile(conversion_ms, .5),
                          percentile(present_ms, .5), percentile(script_ms, .5), percentile(game_ui_ms, .5));
            port::log_stage(timing);
        }
    }
    if (opt.verify_gameplay) {
        try {
            if (!touch_verified)
                throw std::runtime_error("first SDL touch did not reach engine contact 1");
            gameplay_checks(opt.assets);
            if (!port::audio_ready())
                throw std::runtime_error("SDL playback device unavailable");
            if (!port::audio_callback_count() || !port::audio_signal_count())
                throw std::runtime_error("SDL audio callback produced no signal");
            port::pause_audio(true);
            const auto paused_callbacks = port::audio_callback_count();
            SDL_Delay(60);
            if (port::audio_callback_count() != paused_callbacks)
                throw std::runtime_error("audio did not pause");
            port::pause_audio(false);
            SDL_Delay(100);
            if (port::audio_callback_count() == paused_callbacks)
                throw std::runtime_error("audio did not resume");
            port::log_stage("GAMEPLAY_CHECKS_OK scripts callbacks limits inventory menus audio");
        } catch (const std::exception &exception) {
            port::log_stage((std::string("GAMEPLAY_CHECKS_FAIL ") + exception.what()).c_str());
            return 1;
        }
    }
    if (!opt.screenshot.empty()) {
        std::vector<uint8_t> rgba;
        int width = 0, height = 0;
        if (!graphics_device->readback(rgba, error))
            return 1;
        size_t sprite_pixels = 0;
        for (size_t i = 3; i < rgba.size(); i += 4)
            if (rgba[i] == 128)
                ++sprite_pixels;
        std::printf("GPU_CAPTURE sprite_samples=%u\n", static_cast<unsigned>(sprite_pixels));
        if (!graphics_device->readback_display(rgba, width, height, error)) {
            port::log_stage(("GPU_SCREENSHOT_ERROR " + error).c_str());
            return 1;
        }
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface(
            SDL_CreateRGBSurfaceWithFormatFrom(rgba.data(), width, height, 32, width * 4,
                                               SDL_PIXELFORMAT_RGBA32),
            SDL_FreeSurface);
        if (!surface || SDL_SaveBMP(surface.get(), opt.screenshot.c_str()) != 0)
            return 1;
        port::log_stage("SCREENSHOT_GPU_OK");
    }
    char report[512];
    const auto js_stats = port::script_stats();
    std::snprintf(
        report, sizeof(report),
        "SCRIPT_PROFILE calls=%llu errors=%llu total_us=%llu memory_bytes=%lld audio_dropped=%u "
        "audio_callbacks=%u audio_signal=%u",
        static_cast<unsigned long long>(js_stats.calls), static_cast<unsigned long long>(js_stats.errors),
        static_cast<unsigned long long>(js_stats.microseconds), static_cast<long long>(js_stats.memory_bytes),
        port::audio_dropped(), port::audio_callback_count(), port::audio_signal_count());
    port::log_stage(report);
    std::snprintf(report, sizeof(report),
                  "PROFILE frames=%d total_p50=%.2f total_p95=%.2f total_max=%.2f scene_p50=%.2f "
                  "glyph_p50=%.2f upload_p50=%.2f present_p50=%.2f js_p50=%.3f js_p95=%.3f ms",
                  frame, percentile(total_ms, .5), percentile(total_ms, .95), percentile(total_ms, 1),
                  percentile(scene_ms, .5), percentile(conversion_ms, .5), percentile(upload_ms, .5),
                  percentile(present_ms, .5), percentile(script_ms, .5), percentile(script_ms, .95));
    port::log_stage(report);
    if (gpu_scene) {
        std::snprintf(report, sizeof(report),
                      "GPU_PROFILE cpu_wall_ms collect_p50=%.2f collect_p95=%.2f reflection_p50=%.2f "
                      "reflection_p95=%.2f primary_p50=%.2f primary_p95=%.2f overlay_p50=%.2f "
                      "present_wait_p50=%.2f game_ui_p50=%.2f game_ui_p95=%.2f (overlaps_scene=1)",
                      percentile(collect_ms, .5), percentile(collect_ms, .95), percentile(reflection_ms, .5),
                      percentile(reflection_ms, .95), percentile(primary_ms, .5), percentile(primary_ms, .95),
                      percentile(conversion_ms, .5), percentile(present_ms, .5), percentile(game_ui_ms, .5),
                      percentile(game_ui_ms, .95));
        port::log_stage(report);
    }
    if (opt.smoke) {
        const bool complete = opt.seconds
                                  ? now_us() - run_start >= static_cast<uint64_t>(opt.seconds) * 1000000
                                  : frame >= opt.frames;
        std::snprintf(report, sizeof(report),
                      "SMOKE_%s frames=%d movement=%d camera=%d grounded=%d elapsed_s=%.1f",
                      complete && moved && rotated && grounded ? "OK" : "FAIL", frame, moved, rotated,
                      grounded, (now_us() - run_start) / 1000000.0);
        port::log_stage(report);
        if (!complete || !moved || !rotated || !grounded)
            return 1;
    }
    controller.reset();
    if (gpu_scene)
        SetSceneBackend(game->renderer, nullptr);
    gpu_scene.reset();
    port::close_scene();
    port::log_stage("SHUTDOWN_OK scene/resources released");
    return 0;
}
