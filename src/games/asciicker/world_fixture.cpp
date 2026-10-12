#include "games/asciicker/world_fixture.h"
#include "games/asciicker/scene.h"
#include "games/asciicker/mesh_geometry.h"
#include "games/asciicker/terrain_geometry.h"
#include "games/asciicker/ascii_resources.h"
#include "games/asciicker/picking.h"
#include "app/ascii_fixture.h"
#include "platform/graphics/device.h"
#include "platform/engine_hooks.h"
#include "terrain.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
extern Terrain *terrain;
extern World *world;

namespace port::asciicker {
namespace {
double elapsed_ms(uint64_t start) {
    return 1000.0 * static_cast<double>(SDL_GetPerformanceCounter() - start) / SDL_GetPerformanceFrequency();
}
// Adapter uses normalized elevation (upstream z/HEIGHT_SCALE) and x/y units.
// Orthographic height-depth matches upstream's visibility ordering. Perspective,
// camera queries and production camera extraction are not claimed by this test.
engine::Matrix terrain_view(float yaw, float elevation) {
    const float c = std::cos(yaw), s = std::sin(yaw);
    constexpr float kX = 6.0f / 244, kY = 6.0f / 140;
    engine::Matrix matrix = {c * kX,
                             -s * 0.5f * kY,
                             0,
                             0,
                             s * kX,
                             c * 0.5f * kY,
                             0,
                             0,
                             0,
                             0.8660254f * kY,
                             -2.0f / 4096,
                             0,
                             0,
                             0,
                             1,
                             1};
    matrix[12] = -15 * matrix[4];
    matrix[13] = -15 * matrix[5] - elevation * matrix[9];
    return matrix;
}
} // namespace
bool run_world_fixture(const std::string &assets, int frames, const std::string &screenshot,
                       std::string &error) {
    if (frames < 1 || frames > 10000) {
        error = "Terrain fixture requires 1..10000 frames";
        return false;
    }
#ifdef __vita__
    constexpr uint32_t kInit = SDL_INIT_TIMER;
#else
    constexpr uint32_t kInit = SDL_INIT_VIDEO | SDL_INIT_TIMER;
#endif
    if (SDL_Init(kInit) != 0) {
        error = SDL_GetError();
        return false;
    }
    struct Lifetime {
        ~Lifetime() {
            SDL_Quit();
        }
    } lifetime;
    const uint64_t load_begin = SDL_GetPerformanceCounter();
    if (!load_scene(assets, 1000000, error))
        return false;
    struct SceneLifetime {
        ~SceneLifetime() {
            close_scene();
        }
    } scene_lifetime;
    const double load_ms = elapsed_ms(load_begin);
    const uint64_t collect_begin = SDL_GetPerformanceCounter();
    TerrainGeometry snapshot;
    if (!collect_terrain_geometry(terrain, snapshot, error))
        return false;
    MeshGeometry meshes;
    if (!collect_mesh_geometry(world, snapshot.chunks.size(), meshes, error))
        return false;
    std::vector<engine::GeometryInstance> draws;
    draws.reserve(snapshot.chunks.size() + meshes.instances.size());
    for (size_t i = 0; i < snapshot.chunks.size(); ++i)
        draws.push_back({i});
    draws.insert(draws.end(), meshes.instances.begin(), meshes.instances.end());
    const size_t mesh_resources = meshes.resources.size();
    if (mesh_resources != 15 || meshes.instances.size() != 1083 || meshes.triangles != 3635 ||
        meshes.line_segments != 48) {
        error = "GPU world fixture mesh extraction differs from pinned map baseline";
        return false;
    }
    for (auto &resource : meshes.resources)
        snapshot.chunks.push_back(std::move(resource));
    const double collection_ms = elapsed_ms(collect_begin);
    engine::Point3 hit;
    if (!pick_world_surface(terrain, nullptr, {{0, 15, 4096}, {0, 0, -1}}, hit)) {
        error = "Terrain camera ray missed map";
        return false;
    }
    const float elevation = hit[2];
    auto factory = graphics::create_device_factory(error);
    if (!factory)
        return false;
    auto device = factory->create(244, 140, error);
    if (!device)
        return false;
    engine::AsciiResources ascii;
    if (!collect_ascii_resources(assets + "/fonts/cp437_8x8.png", ascii, error) ||
        !device->set_surface(snapshot.surface, error) || !device->set_ascii(ascii, error))
        return false;
    const uint64_t upload_begin = SDL_GetPerformanceCounter();
    if (!device->upload(snapshot.chunks, error))
        return false;
    const double upload_ms = elapsed_ms(upload_begin);
    // No gameplay Render/CPU rasterization, or per-frame geometry uploads.
    // Native fixture serializes target reuse; submission includes display wait.
    std::vector<double> timings;
    timings.reserve(frames);
    for (int frame = 0; frame < frames; ++frame) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_QUIT) {
                error = "Terrain fixture cancelled";
                return false;
            }
        const uint64_t begin = SDL_GetPerformanceCounter();
        if (!device->render(terrain_view(0.78539816f + 0.002f * frame, elevation), draws, error) ||
            !device->present(error))
            return false;
        timings.push_back(elapsed_ms(begin));
    }
    std::vector<uint8_t> rgba;
    if (!device->readback(rgba, error))
        return false; // Single diagnostic capture.
    size_t terrain_pixels = 0;
    for (size_t i = 0; i < rgba.size(); i += 4)
        if (rgba[i + 3] == 0)
            ++terrain_pixels;
    if (terrain_pixels < 244 * 140 / 10) {
        error = "GPU terrain fixture has insufficient real-map coverage";
        return false;
    }
    if (!screenshot.empty()) {
        int display_width = 0, display_height = 0;
        if (!device->readback_display(rgba, display_width, display_height, error))
            return false;
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface(
            SDL_CreateRGBSurfaceWithFormatFrom(rgba.data(), display_width, display_height, 32,
                                               display_width * 4, SDL_PIXELFORMAT_RGBA32),
            SDL_FreeSurface);
        if (!surface || SDL_SaveBMP(surface.get(), screenshot.c_str()) != 0) {
            error = SDL_GetError();
            return false;
        }
    }
    std::sort(timings.begin(), timings.end());
    char message[480];
    std::snprintf(
        message, sizeof(message),
        "GPU_WORLD_FIXTURE_OK backend=%s tiles=%u resources=%u terrain_triangles=%u meshes=%u instances=%u "
        "visible_draws=%u mesh_triangles=%u rendered_lines=%u covered_pixels=%u load_ms=%.2f collect_ms=%.2f "
        "upload_ms=%.2f submit_wait_p50=%.2f submit_wait_p95=%.2f (ASCII diagnostic, not playable game)",
        device->name(), static_cast<unsigned>(snapshot.tiles.size()),
        static_cast<unsigned>(snapshot.chunks.size()), static_cast<unsigned>(snapshot.tiles.size() * 32),
        static_cast<unsigned>(mesh_resources), static_cast<unsigned>(meshes.instances.size()),
        static_cast<unsigned>(device->visible_draws()), static_cast<unsigned>(meshes.triangles),
        static_cast<unsigned>(meshes.line_segments), static_cast<unsigned>(terrain_pixels), load_ms,
        collection_ms, upload_ms, timings[(timings.size() - 1) / 2],
        timings[static_cast<size_t>((timings.size() - 1) * 0.95)]);
    log_stage(message);
    return check_ascii_resolve(*factory, ascii, screenshot.empty() ? "" : screenshot + "-ascii.bmp", error) &&
           check_ascii_resolve(*factory, ascii, "", error, true) &&
           check_ascii_resolve(*factory, ascii, "", error, false, true);
}
} // namespace port::asciicker
