#include "ui/cells.h"
#include "upng.h"
#include "rgba8.h"
#include <algorithm>
#include <cstring>
#include <memory>
namespace port {
namespace {
constexpr uint32_t cube_color(int index) {
    // Match upstream term.cpp Pal(): clamp to the 6x6x6 RGB cube.
    const int value = std::clamp(static_cast<int>(index) - 16, 0, 215);
    return 0xFF000000U | static_cast<uint32_t>((value / 36) * 51) << 16 |
           static_cast<uint32_t>(((value / 6) % 6) * 51) << 8 |
           static_cast<uint32_t>((value % 6) * 51);
}
constexpr std::array<uint32_t, 256> make_palette() {
    std::array<uint32_t, 256> colors{};
    for (int i = 0; i < 256; ++i) colors[i] = cube_color(i);
    return colors;
}
constexpr auto kPalette = make_palette();
constexpr std::array<uint32_t, 256> make_display_palette() {
    std::array<uint32_t, 256> colors{};
    for (int i = 0; i < 256; ++i) {
        const uint32_t rgb = kPalette[i];
        const unsigned r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
        const unsigned luminance = (54 * r + 183 * g + 19 * b + 128) / 256;
        uint32_t graded = 0xFF000000U;
        for (int shift = 0; shift < 24; shift += 8) {
            const unsigned channel = (rgb >> shift) & 255;
            const unsigned muted = (3 * channel + luminance + 2) / 4;
            const unsigned floor = shift == 16 ? 44 : shift == 8 ? 48 : 56;
            graded |= (floor + (196 * muted + 127) / 255) << shift;
        }
        colors[i] = graded;
    }
    return colors;
}
constexpr auto kDisplayPalette = make_display_palette();
}
uint32_t palette_color(uint8_t index) { return kPalette[index]; }
uint32_t display_color(uint8_t index) { return kDisplayPalette[index]; }
bool load_font(const std::string& path, Font& font, std::string& error) {
    std::unique_ptr<upng_t, decltype(&upng_free)> png(upng_new_from_file(path.c_str()), upng_free);
    if (!png || upng_decode(png.get()) != UPNG_EOK || upng_get_width(png.get()) != 128 ||
        upng_get_height(png.get()) != 128 ||
        (upng_get_format(png.get()) != UPNG_INDEX8_RGB && upng_get_format(png.get()) != UPNG_INDEX8_RGBA &&
         upng_get_format(png.get()) != UPNG_LUMINANCE8)) {
        error = "Cannot decode 128x128 font atlas: " + path; return false;
    }
    const auto* data = upng_get_buffer(png.get());
    std::array<uint32_t, 128 * 128> luminance{};
    const uint8_t white[3] = {255, 255, 255};
    ConvertLuminance_UI32_LLZZYYXX(luminance.data(), white,
        static_cast<A3D_ImageFormat>(upng_get_format(png.get())), 128, 128, data,
        upng_get_pal_size(png.get()), upng_get_pal_buffer(png.get()));
    for (int glyph = 0; glyph < 256; ++glyph)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                font[glyph * 64 + y * 8 + x] = luminance[((glyph / 16) * 8 + y) * 128 + (glyph % 16) * 8 + x] >> 24;
    return true;
}
bool rasterize(const engine::Cell* cells, int columns, int rows, const Font& font,
               void* output, std::size_t pitch, std::size_t output_size) {
    if (!cells || !output || columns < 1 || columns > 160 || rows < 1 || rows > 160) return false;
    const auto row_bytes = static_cast<std::size_t>(columns) * 8 * sizeof(uint32_t);
    if (pitch < row_bytes || output_size / pitch < static_cast<std::size_t>(rows) * 8) return false;
    // One small scan replaces half a million unpredictable per-pixel branches.
    // Do not cache by address: callers/tests may change an atlas in place.
    const bool binary = std::all_of(font.begin(), font.end(), [](uint8_t a) { return a == 0 || a == 255; });
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            // Engine cell origin is bottom-left; font atlas origin is top-left.
            const auto& cell = cells[(rows - 1 - y) * columns + x];
            const uint32_t fg = display_color(cell.fg), bg = display_color(cell.bk);
            for (int gy = 0; gy < 8; ++gy) {
                auto* dest = static_cast<uint8_t*>(output) + (y * 8 + gy) * pitch + x * 32;
                if (binary) {
                    uint32_t scanline[8];
                    const auto* coverage = font.data() + cell.gl * 64 + gy * 8;
                    // Exact foreground/background selection, amenable to SIMD.
                    // memcpy retains support for unaligned and padded output.
                    for (int gx = 0; gx < 8; ++gx)
                        scanline[gx] = bg ^ ((fg ^ bg) & (0U - static_cast<uint32_t>(coverage[gx] == 255)));
                    std::memcpy(dest, scanline, sizeof(scanline));
                    continue;
                }
                for (int gx = 0; gx < 8; ++gx) {
                    const unsigned alpha = font[cell.gl * 64 + gy * 8 + gx];
                    // CP437 is normally binary coverage. Preserve exact blending
                    // for alternate antialiased atlases without paying for it
                    // on every opaque/background pixel.
                    uint32_t pixel = bg;
                    if (alpha == 255) pixel = fg;
                    else if (alpha != 0) {
                        pixel = 0xFF000000U;
                        for (int shift = 0; shift < 24; shift += 8) {
                            const unsigned a = (fg >> shift) & 255, b = (bg >> shift) & 255;
                            pixel |= ((a * alpha + b * (255 - alpha) + 127) / 255) << shift;
                        }
                    }
                    std::memcpy(dest + gx * 4, &pixel, sizeof(pixel));
                }
            }
        }
    }
    return true;
}
}
