#include "ui/gpu_presenter.h"
#include <algorithm>

namespace port {
namespace {
SDL_Color color(uint8_t index) {
    const uint32_t rgb = display_color(index);
    return {static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8), static_cast<uint8_t>(rgb), 255};
}
}
bool GpuPresenter::initialize(SDL_Renderer* renderer, const Font& font, int columns, int rows, std::string& error) {
    // Each pass expands to six vertices/cell in Vita SDL's GXM backend.
    // Stay below its U16 index table and 2MiB combined vertex pool.
    if (!renderer || columns < 1 || rows < 1 || columns > 160 || rows > 160 || columns * rows > 8160) {
        error = "GPU presenter grid exceeds supported geometry budget"; return false;
    }
    if (!std::all_of(font.begin(), font.end(), [](uint8_t a) { return a == 0 || a == 255; })) {
        error = "GPU presenter requires a binary-coverage font; use the CPU reference for antialiased atlases"; return false;
    }
    std::array<uint32_t, 128 * 128> pixels{};
    for (int glyph = 0; glyph < 256; ++glyph)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                pixels[((glyph / 16) * 8 + y) * 128 + (glyph % 16) * 8 + x] =
                    0x00FFFFFFU | static_cast<uint32_t>(font[glyph * 64 + y * 8 + x]) << 24;
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> atlas(
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 128, 128), SDL_DestroyTexture);
    if (!atlas || SDL_UpdateTexture(atlas.get(), nullptr, pixels.data(), 128 * 4) != 0 ||
        SDL_SetTextureBlendMode(atlas.get(), SDL_BLENDMODE_BLEND) != 0 ||
        SDL_SetTextureScaleMode(atlas.get(), SDL_ScaleModeNearest) != 0) {
        error = std::string("GPU font atlas: ") + SDL_GetError(); return false;
    }
    const int count = columns * rows;
    vertices_.resize(count * 8);
    indices_.resize(count * 6);
    constexpr int corners[6] = {0, 1, 2, 0, 2, 3};
    for (int cell = 0; cell < count; ++cell) {
        const float x = static_cast<float>((cell % columns) * 8);
        const float y = static_cast<float>((cell / columns) * 8);
        const SDL_FPoint position[4] = {{x, y}, {x + 8, y}, {x + 8, y + 8}, {x, y + 8}};
        for (int corner = 0; corner < 4; ++corner) {
            vertices_[cell * 4 + corner].position = position[corner];
            vertices_[(count + cell) * 4 + corner].position = position[corner];
        }
        for (int i = 0; i < 6; ++i) indices_[cell * 6 + i] = cell * 4 + corners[i];
    }
    renderer_ = renderer; columns_ = columns; rows_ = rows; atlas_ = std::move(atlas); prepared_ = false;
    return true;
}
bool GpuPresenter::prepare(const AnsiCell* cells, std::string& error) {
    if (!renderer_ || !atlas_ || !cells) { error = "GPU presenter is not initialized"; return false; }
    const int count = columns_ * rows_;
    for (int screen_cell = 0; screen_cell < count; ++screen_cell) {
        const auto& cell = cells[(rows_ - 1 - screen_cell / columns_) * columns_ + screen_cell % columns_];
        const SDL_Color fg = color(cell.fg), bg = color(cell.bk);
        const float u = static_cast<float>((cell.gl % 16) * 8) / 128;
        const float v = static_cast<float>((cell.gl / 16) * 8) / 128;
        constexpr float step = 8.0F / 128;
        const SDL_FPoint uv[4] = {{u, v}, {u + step, v}, {u + step, v + step}, {u, v + step}};
        for (int corner = 0; corner < 4; ++corner) {
            vertices_[screen_cell * 4 + corner].color = bg;
            auto& vertex = vertices_[(count + screen_cell) * 4 + corner];
            vertex.color = fg; vertex.tex_coord = uv[corner];
        }
    }
    prepared_ = true;
    return true;
}
bool GpuPresenter::draw(std::string& error) {
    if (!prepared_) { error = "GPU presenter has no prepared cells"; return false; }
    const int count = columns_ * rows_;
    // Different textures prevent GXM from combining passes beyond U16 limits.
    // No per-cell draw calls, texture uploads, or framebuffer readback.
    if (SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE) != 0 ||
        SDL_RenderGeometry(renderer_, nullptr, vertices_.data(), count * 4, indices_.data(), count * 6) != 0 ||
        SDL_RenderGeometry(renderer_, atlas_.get(), vertices_.data() + count * 4, count * 4, indices_.data(), count * 6) != 0) {
        error = std::string("GPU geometry submission: ") + SDL_GetError(); return false;
    }
    return true;
}
}
