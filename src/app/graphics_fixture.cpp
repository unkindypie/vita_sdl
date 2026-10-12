#include "platform/graphics/device.h"
#include "app/graphics_fixture.h"
#include "platform/engine_hooks.h"
#include <SDL.h>
#include <cmath>
#include <cstdio>
#include <limits>
namespace port::graphics {
namespace {
bool pixel_is(const std::vector<uint8_t> &rgba, int width, int x, int y, std::array<int, 3> expected,
              std::string &error) {
    const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
    for (int c = 0; c < 3; ++c)
        if (std::abs(static_cast<int>(rgba[offset + c]) - expected[c]) > 2) {
            char message[180];
            std::snprintf(message, sizeof(message), "GPU pixel (%d,%d) expected %d,%d,%d got %u,%u,%u", x, y,
                          expected[0], expected[1], expected[2], rgba[offset], rgba[offset + 1],
                          rgba[offset + 2]);
            error = message;
            return false;
        }
    return true;
}
double milliseconds(uint64_t start) {
    return 1000.0 * static_cast<double>(SDL_GetPerformanceCounter() - start) / SDL_GetPerformanceFrequency();
}
} // namespace
bool run_geometry_fixture(int frames, const std::string &screenshot, std::string &error) {
    if (frames < 1 || frames > 10000) {
        error = "GPU fixture requires 1..10000 frames";
        return false;
    }
#ifdef __vita__
    constexpr uint32_t kInit = SDL_INIT_TIMER;
#else
    constexpr uint32_t kInit = SDL_INIT_VIDEO | SDL_INIT_TIMER;
#endif
    if (SDL_Init(kInit) != 0) {
        error = std::string("GPU fixture SDL_Init: ") + SDL_GetError();
        return false;
    }
    struct SdlLifetime {
        ~SdlLifetime() {
            SDL_Quit();
        }
    } lifetime;
    // Non-tile-aligned sizes test row padding and context/target recreation.
    const std::array<std::array<int, 2>, 3> sizes = {{{127, 93}, {244, 140}, {127, 93}}};
    double submit_ms = 0, diagnostic_ms = 0;
    auto factory = create_device_factory(error);
    if (!factory)
        return false;
    // Transactional replacement: create/validate the next target before
    // releasing the active one. A failed resize must not destroy a working
    // frame. This also avoids immediate GPU-address recycling during resize.
    std::unique_ptr<Device> previous_device;
    for (const auto &size : sizes) {
        const int width = size[0], height = size[1];
        auto device = factory->create(width, height, error);
        if (!device)
            return false;
        auto geometry = engine::depth_fixture();
        if (device->render(engine::identity_matrix(), error)) {
            error = "Accepted render without upload";
            return false;
        }
        engine::Geometry near, far;
        near.vertices.assign(geometry.vertices.begin(), geometry.vertices.begin() + 3);
        far.vertices.assign(geometry.vertices.begin() + 3, geometry.vertices.end());
        near.indices = far.indices = {0, 1, 2};
        if (!device->upload(std::vector<engine::Geometry>{near, far}, error))
            return false;
        auto invalid = geometry;
        invalid.indices.back() = 100;
        if (device->upload(invalid, error)) {
            error = "Accepted invalid vertex index";
            return false;
        }
        auto bad_matrix = engine::identity_matrix();
        bad_matrix[0] = std::numeric_limits<float>::quiet_NaN();
        if (device->render(bad_matrix, error)) {
            error = "Accepted invalid matrix";
            return false;
        }
        if (!device->render(engine::identity_matrix(), error))
            return false;
        std::vector<uint8_t> baseline, rotated, restored;
        if (!device->readback(baseline, error))
            return false;
        if (baseline.size() != static_cast<size_t>(width) * height * 4) {
            error = "Invalid readback size";
            return false;
        }
        if (!pixel_is(baseline, width, width / 2, height / 2, {230, 77, 64}, error) ||
            !pixel_is(baseline, width, 1, 1, {31, 41, 56}, error) ||
            !pixel_is(baseline, width, width / 2, height * 85 / 100, {64, 153, 230}, error) ||
            !pixel_is(baseline, width, width / 2, height * 5 / 100, {31, 41, 56}, error))
            return false;
        auto translated = engine::identity_matrix();
        translated[12] = 2;
        auto near_shift = engine::identity_matrix();
        near_shift[12] = 0.3f;
        if (!device->render(engine::identity_matrix(), {{0}, {0, near_shift}, {1, translated}}, error))
            return false;
        std::vector<uint8_t> instanced;
        if (!device->readback(instanced, error) ||
            !pixel_is(instanced, width, width / 2, height / 2, {230, 77, 64}, error) ||
            !pixel_is(instanced, width, width * 77 / 100, height / 2, {230, 77, 64}, error) ||
            !pixel_is(instanced, width, width / 2, height * 85 / 100, {31, 41, 56}, error))
            return false;
        if (device->visible_draws() != 2) {
            error = "Incorrect instance visibility/culling";
            return false;
        }
        if (device->render(engine::identity_matrix(), {{100}}, error)) {
            error = "Accepted instance referencing missing GPU resource";
            return false;
        }
        const uint64_t begin = SDL_GetPerformanceCounter();
        for (int i = 0; i < frames; ++i) {
            SDL_Event event;
            while (SDL_PollEvent(&event))
                if (event.type == SDL_QUIT) {
                    error = "GPU fixture cancelled";
                    return false;
                }
            if (!device->render(engine::rotation_y(0.55f * (i + 1) / frames), error) ||
                !device->present(error))
                return false;
        }
        submit_ms += milliseconds(begin);
        const uint64_t read_begin = SDL_GetPerformanceCounter();
        if (!device->readback(rotated, error))
            return false;
        diagnostic_ms += milliseconds(read_begin);
        if (rotated == baseline) {
            error = "GPU rotation had no effect";
            return false;
        }
        std::vector<uint8_t> displayed;
        int display_width = 0, display_height = 0;
        if (!device->readback_display(displayed, display_width, display_height, error))
            return false;
        if (!pixel_is(displayed, display_width, display_width / 2, display_height / 2, {230, 77, 64},
                      error) ||
            !pixel_is(displayed, display_width, 1, 1, {31, 41, 56}, error) ||
            !pixel_is(displayed, display_width, display_width / 2, display_height * 85 / 100, {64, 153, 230},
                      error))
            return false;
        if (!device->render(engine::identity_matrix(), error) || !device->readback(restored, error))
            return false;
        if (restored != baseline) {
            error = "GPU target reuse/depth clear is nondeterministic";
            return false;
        }
        // Dynamic sprites occupy a separate native scene. Both occluded and
        // visible geometry must survive a real depth store/reload, not merely
        // a desktop FBO's persistent depth attachment.
        auto dynamic = near;
        log_stage("GPU_FIXTURE_CHECK dynamic-behind");
        for (auto &vertex : dynamic.vertices) {
            vertex.position[2] = 0.9F;
            vertex.color = {0, 1, 0, 1};
        }
        if (!device->set_dynamic(dynamic, error) || !device->render(engine::identity_matrix(), error) ||
            !device->readback(restored, error) ||
            !pixel_is(restored, width, width / 2, height / 2, {230, 77, 64}, error))
            return false;
        for (auto &vertex : dynamic.vertices)
            vertex.position[2] = -0.9F;
        log_stage("GPU_FIXTURE_CHECK dynamic-front");
        if (!device->set_dynamic(dynamic, error) || !device->render(engine::identity_matrix(), error) ||
            !device->readback(restored, error) ||
            !pixel_is(restored, width, width / 2, height / 2, {0, 255, 0}, error) ||
            !device->set_dynamic({}, error))
            return false;
        auto lit = near;
        log_stage("GPU_FIXTURE_CHECK directional-lighting");
        for (auto &vertex : lit.vertices)
            vertex.surface = {0, 0, 1, 3};
        if (!device->upload(lit, error))
            return false;
        device->set_pass({0, 0, true, 0, {0, 0, 1, 0.25F}});
        if (!device->render(engine::identity_matrix(), error) || !device->readback(restored, error) ||
            !pixel_is(restored, width, width / 2, height / 2, {230, 77, 64}, error))
            return false;
        device->set_pass({0, 0, true, 0, {0, 0, -1, 0.25F}});
        if (!device->render(engine::identity_matrix(), error) || !device->readback(restored, error) ||
            !pixel_is(restored, width, width / 2, height / 2, {0, 0, 0}, error))
            return false;
        device->set_pass({0, 0, true, 0, {1, 0, 0, 0.25F}});
        if (!device->render(engine::identity_matrix(), error) || !device->readback(restored, error) ||
            !pixel_is(restored, width, width / 2, height / 2, {144, 48, 40}, error))
            return false;
        for (auto &vertex : lit.vertices)
            vertex.surface = {1, 0, 0, 3};
        auto rotate_normal = engine::identity_matrix();
        rotate_normal[0] = rotate_normal[5] = 0;
        rotate_normal[1] = 1;
        rotate_normal[4] = -1;
        device->set_pass({0, 0, true, 0, {0, 1, 0, 0.25F}});
        if (!device->upload(lit, error) ||
            !device->render(engine::identity_matrix(), {{0, rotate_normal}}, error) ||
            !device->readback(restored, error) ||
            !pixel_is(restored, width, width / 2, height / 2, {230, 77, 64}, error))
            return false;
        if (!screenshot.empty() && size == sizes[1]) {
            std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface(
                SDL_CreateRGBSurfaceWithFormatFrom(rotated.data(), width, height, 32, width * 4,
                                                   SDL_PIXELFORMAT_RGBA32),
                SDL_FreeSurface);
            if (!surface || SDL_SaveBMP(surface.get(), screenshot.c_str()) != 0) {
                error = std::string("GPU fixture screenshot: ") + SDL_GetError();
                return false;
            }
        }
        char message[180];
        std::snprintf(message, sizeof(message),
                      "GPU_FIXTURE_TARGET_OK backend=%s size=%dx%d depth=pass orientation=pass reuse=pass "
                      "display=pass instances=pass",
                      device->name(), width, height);
        log_stage(message);
        previous_device = std::move(device);
    }
    char message[220];
    std::snprintf(message, sizeof(message),
                  "GPU_FIXTURE_OK frames=%d targets=3 submit_total_ms=%.2f diagnostic_readback_ms=%.2f (not "
                  "a gameplay benchmark)",
                  frames * 3, submit_ms, diagnostic_ms);
    log_stage(message);
    error.clear();
    return true;
}
} // namespace port::graphics
