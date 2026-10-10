#include "app/scene.h"
#include "platform/engine_hooks.h"
#include "platform/input.h"
#include "platform.h"
#include "scripting/probe.h"
#include "ui/cells.h"
#include "app/self_test.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <vector>
namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
uint64_t stamp() { return 1000000; }
}
int main(int argc, char** argv) {
    try {
        require(argc == 4, "expected asset root, manifest and isolated config path");
        port::set_config_path(argv[3]);
        require(port::palette_color(16) == 0xFF000000, "black");
        require(port::palette_color(196) == 0xFFFF0000, "red");
        require(port::palette_color(46) == 0xFF00FF00, "green");
        require(port::palette_color(21) == 0xFF0000FF, "blue");
        require(port::palette_color(231) == 0xFFFFFFFF, "white");
        port::Font font{};
        font[65 * 64] = 255;
        const AnsiCell cells[2] = {{231, 21, 32, 0}, {196, 16, 65, 0}};
        std::vector<uint32_t> pixels(10 * 16, 0x12345678);
        require(port::rasterize(cells, 1, 2, font, pixels.data(), 40, pixels.size() * 4), "padded raster");
        require(pixels[0] == 0xFFFF0000 && pixels[1] == 0xFF000000, "glyph orientation/foreground");
        require(pixels[80] == 0xFF0000FF, "bottom cell/background");
        require(pixels[8] == 0x12345678, "row padding preserved");
        // Check every coverage value, including fast-path endpoints, with
        // deliberately unaligned output. The original blend must be unchanged.
        const AnsiCell blend_cell{196, 21, 65, 0};
        std::vector<uint8_t> unaligned(8 * 8 * 4 + 1);
        for (int alpha = 0; alpha <= 255; ++alpha) {
            font.fill(static_cast<uint8_t>(alpha));
            require(port::rasterize(&blend_cell, 1, 1, font, unaligned.data() + 1, 32, unaligned.size() - 1), "blend raster");
            const uint32_t expected = 0xFF000000U | static_cast<uint32_t>(alpha) << 16 |
                                      static_cast<uint32_t>(255 - alpha);
            for (int pixel = 0; pixel < 64; ++pixel) {
                uint32_t actual = 0;
                std::memcpy(&actual, unaligned.data() + 1 + pixel * 4, sizeof(actual));
                require(actual == expected, "binary/antialiased coverage unchanged");
            }
        }
        require(!port::rasterize(cells, 1, 2, font, pixels.data(), 31, pixels.size() * 4), "bad pitch rejected");
        require(!port::rasterize(cells, 1, 2, font, pixels.data(), 40, 10), "short output rejected");
        require(!port::rasterize(cells, 161, 2, font, pixels.data(), 40, pixels.size() * 4), "invalid dimensions");
        require(port::engine_key(SDLK_w) == A3D_W, "keyboard mapping");
        require(port::engine_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) == 14, "dpad mapping");
        require(port::engine_button(255) == -1, "unknown button");
        require(port::engine_touch_contact(0)==1 && port::engine_touch_contact(2)==3 && port::engine_touch_contact(3)==-1,"touch reserves mouse contact zero");
        require(port::axis_value(-5999) == 0 && port::axis_value(-32768) == -32768, "deadzone");
        std::string error;
        require(port::probe_quickjs(error), error.c_str());
        require(port::validate_assets(argv[1], argv[2], error), error.c_str());
        require(!port::validate_assets("/missing-assets", argv[2], error), "missing files rejected");
        require(port::load_font(std::string(argv[1]) + "/fonts/cp437_8x8.png", font, error), error.c_str());
        MakeStamp = stamp;
        // Exercise full load/render/free twice to catch stale global pointers.
        for (int iteration = 0; iteration < 2; ++iteration) {
            require(port::load_scene(argv[1], 1000000, error), error.c_str());
            game->OnSize(960, 544, 8, 8);
            std::vector<AnsiCell> frame(120 * 68);
            for (int i = 1; i <= 90; ++i) game->Render(1000000 + i * 16667ULL, frame.data(), 120, 68);
            int nonblank = 0;
            for (const auto& cell : frame) if (cell.gl != ' ' && cell.gl != 0) ++nonblank;
            require(nonblank > 500, "actual scene contains glyphs");
            require(std::isfinite(game->player.pos[2]), "physics position finite");
            gameplay_checks(argv[1]);
            game->OnKeyb(KEYB_DOWN, A3D_W);
            require(game->input.IsKeyDown(A3D_W), "press delivered");
            game->OnKeyb(KEYB_UP, A3D_W);
            require(!game->input.IsKeyDown(A3D_W), "release delivered");
            game->OnKeyb(KEYB_DOWN, A3D_SPACE);
            require(game->input.jump, "jump request delivered");
            game->OnKeyb(KEYB_UP, A3D_SPACE);
            game->OnKeyb(KEYB_DOWN, A3D_W);
            game->OnFocus(false);
            require(!game->input.IsKeyDown(A3D_W), "focus loss clears held movement");
            require(game->input.size[0] == 960 && game->input.size[1] == 544, "focus loss preserves dimensions");
            game->OnFocus(true);
            game->Render(game->stamp + 16667ULL, frame.data(), 120, 68);
            require(std::isfinite(game->player.pos[2]), "foreground render resumes");
            port::close_scene();
            require(game == nullptr, "scene released");
        }
        std::puts("TESTS_OK presenter input quickjs assets scene repeated-teardown");
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "TEST_FAILURE %s\n", e.what()); return 1; }
}
