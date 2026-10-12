#pragma once
#include "engine/render/cell.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace port {
constexpr int kGlyphSize = 8;
using Font = std::array<uint8_t, 256 * kGlyphSize * kGlyphSize>;
uint32_t palette_color(uint8_t index);
// Presentation-only pastel grading; engine material/light indices stay intact.
uint32_t display_color(uint8_t index);
bool load_font(const std::string& path, Font& font, std::string& error);
bool rasterize(const engine::Cell* cells, int columns, int rows, const Font& font,
               void* output, std::size_t pitch, std::size_t output_size);
}
