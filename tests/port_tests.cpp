#include "games/asciicker/scene.h"
#include "platform/engine_hooks.h"
#include "platform/input.h"
#include "platform.h"
#include "scripting/probe.h"
#include "ui/cells.h"
#include "ui/raster_math.h"
#include "app/self_test.h"
#include "simulation_view.h"
#include <algorithm>
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
        for (int i = -65536; i <= 65536; ++i) {
            const float f = i / 32.0F;
            const double d = i / 31.0;
            require(port::floor_to_int(f) == static_cast<int>(std::floor(f)), "float raster floor exact");
            require(port::floor_to_int(d) == static_cast<int>(std::floor(d)), "double raster floor exact");
        }
        for (double boundary : {-2147483648.0, -100.0, -1.0, -0.0, 0.0, 1.0, 100.0, 2147483646.0}) {
            for (double delta : {-0.00001, 0.0, 0.00001}) {
                const double value = boundary + delta;
                if (value >= -2147483648.0)
                    require(port::floor_to_int(value) == static_cast<int>(std::floor(value)), "raster floor boundary");
            }
        }
        require(port::floor_to_int(-1.0e-30F) == -1 && port::floor_to_int(1.0e-30F) == 0, "small signed raster floor");
        require(port::palette_color(16) == 0xFF000000, "black");
        require(port::palette_color(196) == 0xFFFF0000, "red");
        require(port::palette_color(46) == 0xFF00FF00, "green");
        require(port::palette_color(21) == 0xFF0000FF, "blue");
        require(port::palette_color(231) == 0xFFFFFFFF, "white");
        require(port::display_color(16) == 0xFF2C3038, "pastel shadows lifted to slate");
        require(port::display_color(231) == 0xFFF0F4FC, "pastel highlights bounded");
        for (int channel = 0; channel < 3; ++channel) {
            unsigned previous = 0;
            for (int level = 0; level < 6; ++level) {
                const int stride = channel == 0 ? 1 : channel == 1 ? 6 : 36;
                const unsigned value = (port::display_color(16 + level * stride) >> (channel * 8)) & 255;
                require(value > previous, "pastel channel ramps preserve shading order");
                previous = value;
            }
        }
        port::Font font{};
        font[65 * 64] = 255;
        const AnsiCell cells[2] = {{231, 21, 32, 0}, {196, 16, 65, 0}};
        std::vector<uint32_t> pixels(10 * 16, 0x12345678);
        require(port::rasterize(cells, 1, 2, font, pixels.data(), 40, pixels.size() * 4), "padded raster");
        require(pixels[0] == port::display_color(196) && pixels[1] == port::display_color(16), "glyph orientation/foreground");
        require(pixels[80] == port::display_color(21), "bottom cell/background");
        require(pixels[8] == 0x12345678, "row padding preserved");
        // Exercise every palette index and glyph with mixed binary coverage,
        // unaligned output and padded rows. Compare with scalar selection.
        std::array<AnsiCell, 256> varied{};
        for (int i = 0; i < 256; ++i) {
            varied[i] = {static_cast<uint8_t>(i), static_cast<uint8_t>(255 - i), static_cast<uint8_t>(i), 0};
            const int cube = std::clamp(i - 16, 0, 215);
            const uint32_t expected = 0xFF000000U | static_cast<uint32_t>(cube / 36 * 51) << 16 |
                static_cast<uint32_t>(cube / 6 % 6 * 51) << 8 | static_cast<uint32_t>(cube % 6 * 51);
            require(port::palette_color(static_cast<uint8_t>(i)) == expected, "palette lookup matches cube");
            const unsigned r = (expected >> 16) & 255, g = (expected >> 8) & 255, b = expected & 255;
            const unsigned luminance = (54 * r + 183 * g + 19 * b + 128) / 256;
            uint32_t graded = 0xFF000000U;
            for (int shift = 0; shift < 24; shift += 8) {
                const unsigned muted = (3 * ((expected >> shift) & 255) + luminance + 2) / 4;
                graded |= ((shift == 16 ? 44U : shift == 8 ? 48U : 56U) + (196 * muted + 127) / 255) << shift;
            }
            require(port::display_color(static_cast<uint8_t>(i)) == graded, "all pastel palette entries");
            for (int p = 0; p < 64; ++p) font[i * 64 + p] = (i * 13 + p * 7) % 11 < 5 ? 255 : 0;
        }
        constexpr int test_pitch = 16 * 8 * 4 + 7;
        std::vector<uint8_t> varied_pixels(test_pitch * 128 + 1, 0xA5);
        require(port::rasterize(varied.data(), 16, 16, font, varied_pixels.data() + 1,
                               test_pitch, varied_pixels.size() - 1), "binary frame raster");
        for (int y = 0; y < 128; ++y) {
            for (int x = 0; x < 128; ++x) {
                const auto& cell = varied[(15 - y / 8) * 16 + x / 8];
                const uint8_t a = font[cell.gl * 64 + (y % 8) * 8 + x % 8];
                const uint32_t expected = port::display_color(a ? cell.fg : cell.bk);
                uint32_t actual = 0;
                std::memcpy(&actual, varied_pixels.data() + 1 + y * test_pitch + x * 4, 4);
                require(actual == expected, "branchless binary raster matches scalar");
            }
            for (int p = 512; p < test_pitch; ++p)
                require(varied_pixels[1 + y * test_pitch + p] == 0xA5, "binary frame padding untouched");
        }
        // Check every coverage value, including fast-path endpoints, with
        // deliberately unaligned output. The original blend must be unchanged.
        const AnsiCell blend_cell{196, 21, 65, 0};
        std::vector<uint8_t> unaligned(8 * 8 * 4 + 1);
        for (int alpha = 0; alpha <= 255; ++alpha) {
            font.fill(static_cast<uint8_t>(alpha));
            require(port::rasterize(&blend_cell, 1, 1, font, unaligned.data() + 1, 32, unaligned.size() - 1), "blend raster");
            uint32_t expected = 0xFF000000U;
            for (int shift = 0; shift < 24; shift += 8) {
                const unsigned fg = (port::display_color(196) >> shift) & 255;
                const unsigned bg = (port::display_color(21) >> shift) & 255;
                expected |= ((fg * alpha + bg * (255 - alpha) + 127) / 255) << shift;
            }
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
            SimulationView view;
            require(port::load_scene(argv[1], 1000000, error), error.c_str());
            SetSceneBackend(game->renderer, &view);
            game->OnSize(960, 544, 8, 8);
            std::vector<AnsiCell> frame(120 * 68);
            for (int i = 1; i <= 90; ++i) game->Render(1000000 + i * 16667ULL, frame.data(), 120, 68);
            int nonblank = 0;
            for (const auto& cell : frame) if (cell.gl != ' ' && cell.gl != 0) ++nonblank;
            require(nonblank > 0 && view.calls == 90 && !GetSceneBackendError(game->renderer),
                    "independent UI and exactly-once simulation view dispatch");
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
            SetSceneBackend(game->renderer, nullptr);
            port::close_scene();
            require(game == nullptr, "scene released");
        }
        std::puts("TESTS_OK presenter input quickjs assets scene repeated-teardown");
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "TEST_FAILURE %s\n", e.what()); return 1; }
}
