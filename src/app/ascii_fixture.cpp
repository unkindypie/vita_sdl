#include "app/ascii_fixture.h"
#include "platform/engine_hooks.h"
#include <SDL.h>
#include <cmath>
#include <cstdio>
#include <memory>
namespace port {
bool check_ascii_resolve(graphics::DeviceFactory &factory, const engine::AsciiResources &resources,
                         const std::string &screenshot, std::string &error, bool mixed) {
    auto device = factory.create(36, 36, error);
    if (!device)
        return false;
    engine::Geometry geometry;
    // One material per cell, all 256 IDs. Each quad covers exactly two scene
    // samples; padding is excluded by the resolve. No CPU scene rasterization.
    const int extent = mixed ? 32 : 16;
    const int step = mixed ? 1 : 2;
    for (int y = 0; y < extent; ++y)
        for (int x = 0; x < extent; ++x) {
            const int cx = mixed ? x / 2 : x, cy = mixed ? y / 2 : y;
            // Regression: two matching lower samples must beat two different
            // upper samples. This specifically catches the missing c==d vote.
            const int offset = mixed ? (y % 2 ? 2 : x % 2) : 0;
            const float left = (static_cast<float>(step * x) + 2) / 36 * 2 - 1,
                        right = (static_cast<float>(step * x) + 2 + step) / 36 * 2 - 1;
            const float top = 1 - (static_cast<float>(step * y) + 2) / 36 * 2,
                        bottom = 1 - (static_cast<float>(step * y) + 2 + step) / 36 * 2;
            std::array<float, 4> metadata = {static_cast<float>((cy * 16 + cx + offset) % 256) / 255,
                                             cx / 255.0f, (cy % 4) / 255.0f, 0};
            if (mixed && x % 2 == 0 && y % 2 == 0) {
                // Its red matches the winning terrain ID, but this sample is
                // direct RGB. Cross-domain equality must not steal the vote.
                metadata[0] = static_cast<float>((cy * 16 + cx + 2) % 256) / 255;
                metadata[3] = 1;
            }
            const uint16_t base = static_cast<uint16_t>(geometry.vertices.size());
            geometry.vertices.push_back({{left, bottom, 0}, metadata});
            geometry.vertices.push_back({{right, bottom, 0}, metadata});
            geometry.vertices.push_back({{left, top, 0}, metadata});
            geometry.vertices.push_back({{right, top, 0}, metadata});
            for (uint16_t index : {0, 1, 2, 2, 1, 3})
                geometry.indices.push_back(base + index);
        }
    if (!device->upload(geometry, error) || !device->set_ascii(resources, error) ||
        !device->render(engine::identity_matrix(), error) || !device->present(error))
        return false;
    std::vector<uint8_t> rgba;
    int width = 0, height = 0;
    if (!device->readback_display(rgba, width, height, error))
        return false;
    size_t checked = 0;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const double u = (x + 0.5) * 16 / width, v = (y + 0.5) * 16 / height;
            // Avoid exactly-on-texel boundaries: permitted GPU interpolation
            // rounding can select either adjacent texel at those positions only.
            if (std::abs(u * 8 - std::round(u * 8)) < 0.001 || std::abs(v * 8 - std::round(v * 8)) < 0.001)
                continue;
            const int cx = static_cast<int>(u), cy = static_cast<int>(v), gx = static_cast<int>((u - cx) * 8),
                      gy = static_cast<int>((v - cy) * 8);
            const auto *fg = resources.materials.data() +
                             (((cy * 16 + cx + (mixed ? 2 : 0)) % 256) * 128 + cx * 8 + (cy % 4) * 2) * 4;
            const auto *bg = fg + 4;
            const int glyph = fg[3];
            const unsigned coverage =
                resources.font[(((glyph / 16) * 8 + gy) * 128 + (glyph % 16) * 8 + gx) * 4 + 3];
            for (int channel = 0; channel < 3; ++channel) {
                const int expected = (bg[channel] * (255 - coverage) + fg[channel] * coverage + 127) / 255;
                const int actual = rgba[(y * width + x) * 4 + channel];
                if (std::abs(actual - expected) > 1) {
                    char message[200];
                    std::snprintf(message, sizeof(message),
                                  "ASCII resolve mismatch x=%d y=%d material=%d glyph=%d channel=%d "
                                  "expected=%d actual=%d",
                                  x, y, cy * 16 + cx, glyph, channel, expected, actual);
                    error = message;
                    return false;
                }
            }
            ++checked;
        }
    if (!screenshot.empty()) {
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface(
            SDL_CreateRGBSurfaceWithFormatFrom(rgba.data(), width, height, 32, width * 4,
                                               SDL_PIXELFORMAT_RGBA32),
            SDL_FreeSurface);
        if (!surface || SDL_SaveBMP(surface.get(), screenshot.c_str()) != 0) {
            error = SDL_GetError();
            return false;
        }
    }
    char message[200];
    std::snprintf(
        message, sizeof(message),
        "GPU_ASCII_FIXTURE_OK backend=%s materials=256 shades=16 elevations=4 mixed=%d pixels_checked=%u",
        device->name(), mixed ? 1 : 0, static_cast<unsigned>(checked));
    log_stage(message);
    if (!mixed) {
        // A front sprite preserves the terrain under transparent channels;
        // a rear sprite is rejected by depth. UI is a separate top-down upload.
        engine::Geometry dynamic;
        for (int half = 0; half < 2; ++half) {
            const float left = (2.0F + half * 16) / 36 * 2 - 1;
            const float right = (18.0F + half * 16) / 36 * 2 - 1;
            const float bottom = 2.0F / 36 * 2 - 1, top = 34.0F / 36 * 2 - 1;
            const float depth = half ? 0.5F : -0.5F;
            const std::array<float, 4> metadata{231 / 255.0F, 1, 65 / 255.0F, 128 / 255.0F};
            const uint16_t first = static_cast<uint16_t>(dynamic.vertices.size());
            dynamic.vertices.insert(dynamic.vertices.end(), {{{left, bottom, depth}, metadata},
                                                             {{right, bottom, depth}, metadata},
                                                             {{left, top, depth}, metadata},
                                                             {{right, top, depth}, metadata}});
            for (int offset : {0, 1, 2, 2, 1, 3})
                dynamic.indices.push_back(first + offset);
        }
        std::vector<engine::Cell> ui(256, engine::Cell{255, 255, 0, 0});
        for (int y = 8; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                ui[y * 16 + x] = {17, 16, 66, 0};
        if (!device->set_dynamic(dynamic, error) || !device->set_overlay(ui.data(), 16, 16, error) ||
            !device->render(engine::identity_matrix(), error) || !device->present(error) ||
            !device->readback_display(rgba, width, height, error))
            return false;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const double u = (x + 0.5) * 16 / width, v = (y + 0.5) * 16 / height;
                if (std::abs(u * 8 - std::round(u * 8)) < 0.001 ||
                    std::abs(v * 8 - std::round(v * 8)) < 0.001)
                    continue;
                const int cx = static_cast<int>(u), cy = static_cast<int>(v);
                const int gx = static_cast<int>((u - cx) * 8), gy = static_cast<int>((v - cy) * 8);
                const auto *fg =
                    resources.materials.data() + ((cy * 16 + cx) * 128 + cx * 8 + (cy % 4) * 2) * 4;
                const auto *bg = fg + 4;
                auto coverage = [&](int glyph) {
                    return resources.font[(((glyph / 16) * 8 + gy) * 128 + (glyph % 16) * 8 + gx) * 4 + 3];
                };
                for (int channel = 0; channel < 3; ++channel) {
                    int expected =
                        (bg[channel] * (255 - coverage(fg[3])) + fg[channel] * coverage(fg[3]) + 127) / 255;
                    if (cx < 8)
                        expected = (expected * (255 - coverage(65)) +
                                    resources.font[231 * 4 + channel] * coverage(65) + 127) /
                                   255;
                    if (cy < 8)
                        expected = (resources.font[16 * 4 + channel] * (255 - coverage(66)) +
                                    resources.font[17 * 4 + channel] * coverage(66) + 127) /
                                   255;
                    if (std::abs(rgba[(y * width + x) * 4 + channel] - expected) > 1) {
                        error = "GPU sprite depth/transparency or UI overlay composition mismatch at " +
                                std::to_string(x) + "," + std::to_string(y);
                        return false;
                    }
                }
            }
        log_stage("GPU_COMPOSITION_OK front/rear sprite depth transparent background independent UI");

        // Dirty terrain uploads must retain the atlas row stride and leave
        // neighbouring texels intact. Read raw material IDs, not shaded RGB.
        engine::SurfaceTexture surface;
        surface.width = surface.height = 4;
        surface.rgba.resize(4 * 4 * 4, 0);
        for (size_t i = 0; i < surface.rgba.size(); i += 4) {
            surface.rgba[i] = 23;
            surface.rgba[i + 3] = 255;
        }
        for (auto &vertex : geometry.vertices)
            vertex.surface = {0.375F, 0.625F, 1, 1};
        if (!device->set_dynamic({}, error) || !device->set_surface(surface, error) ||
            !device->upload(geometry, error))
            return false;
        surface.rgba[(2 * 4 + 1) * 4] = 47;
        std::string rejected;
        if (device->update_surface(surface, 4, 0, 1, 1, rejected) || rejected.empty()) {
            error = "GPU surface update accepted an out-of-bounds region";
            return false;
        }
        if (!device->update_surface(surface, 1, 2, 1, 1, error) ||
            !device->render(engine::identity_matrix(), error) || !device->readback(rgba, error))
            return false;
        if (rgba[(18 * 36 + 18) * 4] != 47) {
            error = "GPU surface region upload used the wrong atlas stride/texel";
            return false;
        }
        for (auto &vertex : geometry.vertices)
            vertex.surface[0] = 0.625F;
        if (!device->upload(geometry, error) || !device->render(engine::identity_matrix(), error) ||
            !device->readback(rgba, error))
            return false;
        if (rgba[(18 * 36 + 18) * 4] != 23) {
            error = "GPU surface region upload overwrote a neighbouring texel";
            return false;
        }
        log_stage("GPU_SURFACE_UPDATE_OK region bounds row_stride neighbouring_texels");
    }
    return true;
}
} // namespace port
