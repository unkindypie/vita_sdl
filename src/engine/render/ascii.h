#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>
#include "engine/render/cell.h"

namespace engine {
inline bool pack_overlay(const Cell *cells, int columns, int rows, std::vector<uint8_t> &pixels) {
    if (!cells || columns < 1 || rows < 1 || columns > 128 || rows > 128)
        return false;
    pixels.assign(128 * 128 * 4, 0);
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < columns; ++x) {
            const auto &cell = cells[(rows - 1 - y) * columns + x];
            auto *texel = pixels.data() + (y * 128 + x) * 4;
            texel[0] = cell.fg;
            texel[1] = cell.bk;
            texel[2] = cell.gl;
            texel[3] = (cell.fg != 255 || cell.bk != 255) ? 255 : 0;
        }
    return true;
}
struct SurfaceTexture {
    int width = 0, height = 0;
    std::vector<uint8_t> rgba;
    bool valid() const {
        return width > 0 && height > 0 && width <= 2048 && height <= 2048 &&
               rgba.size() == static_cast<std::size_t>(width) * height * 4;
    }
};
// Point-sampled RGBA8 LUT. For each material row, shade * 8 + elevation * 2
// contains foreground RGB/glyph, followed by background RGB/flags. Byte values
// are exact; no filtered material IDs or glyph interpolation is permitted.
struct AsciiResources {
    static constexpr int kMaterialWidth = 128;
    static constexpr int kMaterialHeight = 256;
    static constexpr int kFontWidth = 128;
    std::array<uint8_t, kMaterialWidth * kMaterialHeight * 4> materials{};
    std::array<uint8_t, kFontWidth * kFontWidth * 4> font{};
};
// Scene metadata: R=material, G=lighting shade [0,15], B=elevation [0,3],
// A=0. Direct mesh colors have A=255 (their ASCII edge resolve is separate).
// The shader source is shared by the two API wrappers to prevent divergent
// glyph sampling, material addressing and four-sample selection.
} // namespace engine
