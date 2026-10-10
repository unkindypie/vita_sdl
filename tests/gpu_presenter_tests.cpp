#include "ui/gpu_presenter.h"
#include <algorithm>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
}
int main(int argc, char** argv) {
    try {
        require(argc == 2 || (argc == 3 && std::string(argv[2]) == "--accelerated"), "expected asset root [--accelerated]");
        const bool accelerated = argc == 3;
        require(SDL_Init(SDL_INIT_TIMER | (accelerated ? SDL_INIT_VIDEO : 0)) == 0, SDL_GetError());
        struct Lifetime { ~Lifetime() { SDL_Quit(); } } lifetime;
        constexpr int columns = 120, rows = 68, width = columns * 8, height = rows * 8;
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface(
            SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
        require(surface != nullptr, SDL_GetError());
        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(nullptr, SDL_DestroyWindow);
        if (accelerated) {
            window.reset(SDL_CreateWindow("GPU presenter parity", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                width, height, SDL_WINDOW_HIDDEN));
            require(window != nullptr, SDL_GetError());
        }
        std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(accelerated ?
            SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED) : SDL_CreateSoftwareRenderer(surface.get()), SDL_DestroyRenderer);
        require(renderer != nullptr, SDL_GetError());
        SDL_RendererInfo info{};
        require(SDL_GetRendererInfo(renderer.get(), &info) == 0, SDL_GetError());
        if (accelerated) require((info.flags & SDL_RENDERER_ACCELERATED) != 0, "actual accelerated backend required");
        std::printf("GEOMETRY_BACKEND %s accelerated=%d\n", info.name, accelerated);
        port::GpuPresenter gpu;
        port::Font font{};
        std::string error;
        std::vector<AnsiCell> cells(columns * rows);
        std::vector<uint32_t> actual(width * height), expected(width * height);
        require(!gpu.draw(error), "uninitialized draw rejected");
        require(!gpu.prepare(cells.data(), error), "uninitialized preparation rejected");
        require(!gpu.initialize(renderer.get(), font, 160, 160, error), "GXM geometry budget enforced");
        require(!gpu.initialize(renderer.get(), font, 0, rows, error), "empty grid rejected");
        font[0] = 128;
        require(!gpu.initialize(renderer.get(), font, columns, rows, error), "nonbinary atlas rejected");
        for (int atlas = 0; atlas < 2; ++atlas) {
            if (atlas == 0) {
                for (std::size_t i = 0; i < font.size(); ++i) font[i] = i * 7 % 11 < 5 ? 255 : 0;
            } else require(port::load_font(std::string(argv[1]) + "/fonts/cp437_8x8.png", font, error), error.c_str());
            require(gpu.initialize(renderer.get(), font, columns, rows, error), error.c_str());
            for (int frame = 0; frame < 2; ++frame) {
                for (std::size_t i = 0; i < cells.size(); ++i) cells[i] = {
                    static_cast<uint8_t>(i + frame), static_cast<uint8_t>(255 - i + frame),
                    static_cast<uint8_t>(i * 17 + frame * 11), 0};
                require(SDL_RenderClear(renderer.get()) == 0, SDL_GetError());
                require(gpu.prepare(cells.data(), error), error.c_str());
                require(gpu.draw(error), error.c_str());
                require(SDL_RenderReadPixels(renderer.get(), nullptr, SDL_PIXELFORMAT_ARGB8888,
                    actual.data(), width * 4) == 0, SDL_GetError());
                require(port::rasterize(cells.data(), columns, rows, font, expected.data(), width * 4,
                    expected.size() * 4), "CPU reference raster");
                require(actual == expected, "actual geometry framebuffer differs from CPU reference");
            }
        }
        std::puts("GPU_GEOMETRY_PARITY_OK actual-renderer-readback binary glyphs palette orientation repeated frames real atlas");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GPU_GEOMETRY_PARITY_FAIL %s\n", error.what()); return 1;
    }
}
