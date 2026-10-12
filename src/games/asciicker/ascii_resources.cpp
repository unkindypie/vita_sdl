#include "games/asciicker/ascii_resources.h"
#include "ui/cells.h"
#include "render.h"
#include <utility>
extern Material mat[256];
namespace port::asciicker {
namespace {
uint8_t quantize(const uint8_t rgb[3]) {
    return static_cast<uint8_t>(16 + ((rgb[0] + 25) / 51) * 36 + ((rgb[1] + 25) / 51) * 6 +
                                (rgb[2] + 25) / 51);
}
void write_color(uint8_t *destination, const uint8_t rgb[3], uint8_t alpha) {
    const uint32_t graded = display_color(quantize(rgb));
    destination[0] = graded >> 16;
    destination[1] = graded >> 8;
    destination[2] = graded;
    destination[3] = alpha;
}
} // namespace
bool collect_ascii_resources(const std::string &font_path, engine::AsciiResources &output,
                             std::string &error) {
    Font font;
    if (!load_font(font_path, font, error))
        return false;
    engine::AsciiResources result;
    for (int material = 0; material < 256; ++material)
        for (int shade = 0; shade < 16; ++shade)
            for (int elevation = 0; elevation < 4; ++elevation) {
                const auto &cell = mat[material].shade[elevation][shade];
                auto *texel = result.materials.data() + (material * 128 + shade * 8 + elevation * 2) * 4;
                write_color(texel, cell.fg, cell.gl);
                write_color(texel + 4, cell.bg, cell.flags);
            }
    for (int glyph = 0; glyph < 256; ++glyph)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                auto *texel = result.font.data() + (((glyph / 16) * 8 + y) * 128 + (glyph % 16) * 8 + x) * 4;
                texel[0] = texel[1] = texel[2] = 255;
                texel[3] = font[glyph * 64 + y * 8 + x];
            }
    // RGB is unused by glyph coverage (alpha). Store the shared display
    // palette in its first two rows, avoiding another texture/sampler.
    for (int index=0;index<256;++index) {
        const uint32_t rgb=display_color(static_cast<uint8_t>(index));
        result.font[index*4]=rgb>>16; result.font[index*4+1]=rgb>>8; result.font[index*4+2]=rgb;
    }
    output = std::move(result);
    return true;
}
} // namespace port::asciicker
