#include "render.h"
#include "engine/render/scene_backend.h"
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct ProbeBackend final : engine::SceneBackend {
    int calls = 0;
    bool fail_render = false, fail_camera = false, nonfinite_pick = false;
    engine::SceneFrame captured{};
    bool render(const engine::SceneFrame& frame, engine::CellTarget target, std::string& error) override {
        ++calls; captured = frame;
        if (fail_render) { error = "injected shader/pass failure"; return false; }
        target.data[0] = {231, 16, 65, 0};
        return true;
    }
    bool project(const engine::Position& world, engine::ScreenPoint& screen) const override {
        screen = {static_cast<int>(world[0]), static_cast<int>(world[1]), 77};
        return !fail_camera;
    }
    bool unproject(const engine::ScreenPoint& screen, engine::Position& world) const override {
        world = {static_cast<float>(screen.x), static_cast<float>(screen.y), static_cast<float>(screen.depth)};
        return !fail_camera;
    }
    bool pick_surface(int x, int y, engine::Position& world) const override {
        world = {static_cast<float>(x), static_cast<float>(y),
            nonfinite_pick ? std::numeric_limits<float>::quiet_NaN() : 88.0F};
        return !fail_camera;
    }
};
}
int main() {
    try {
        // Backend outlives both opaque renderers. No singleton, borrowed data or
        // graphics context is shared accidentally between renderer instances.
        ProbeBackend first, second;
        using RendererOwner = std::unique_ptr<Renderer, decltype(&DeleteRenderer)>;
        RendererOwner a(CreateRenderer(1), DeleteRenderer), b(CreateRenderer(2), DeleteRenderer);
        SetSceneBackend(a.get(), &first); SetSceneBackend(b.get(), &second);
        AnsiCell cells[4]{};
        float pos[3] = {-12, 34, 56}, light[4] = {1, 0, 1, 0.5F};
        int shift[2] = {7, -2}, screen[3] = {9, 9, 9};
        require(!ProjectCoords(a.get(), pos, screen), "camera unavailable before first frame");
        require(screen[0] == 9, "rejected projection leaves output unchanged");
        auto draw = [&](Renderer* renderer, int width = 2) {
            // Null world/terrain deliberately: an accidental CPU render would
            // not be a legitimate fallback for this injected scene backend.
            Render(renderer, 1234567, nullptr, nullptr, 55, 1.25F, 45, pos, light,
                   width, 2, cells, nullptr, shift, true);
        };
        draw(a.get());
        require(first.calls == 1 && second.calls == 0, "scene dispatched once to the correct backend");
        require(!GetSceneBackendError(a.get()) && cells[0].gl == 65, "backend cell target delivered");
        require(first.captured.timestamp_us == 1234567 && first.captured.camera.position[0] == -12 &&
                first.captured.camera.zoom == 1.25F && first.captured.camera.cell_shift[1] == -2 &&
                first.captured.water_height == 55 && first.captured.light[3] == 0.5F &&
                first.captured.camera.perspective, "frame metadata copied without conversion");
        require(ProjectCoords(a.get(), pos, screen) && screen[0] == -12 && screen[2] == 77, "backend projection delegated");
        float world[3] = {};
        require(UnprojectCoords3D(a.get(), screen, world) && world[2] == 77, "known-height unprojection delegated");
        const int pick[2] = {1, 0}, outside[2] = {-1, 0};
        require(UnprojectCoords2D(a.get(), pick, world) && world[2] == 88, "collision surface picking delegated");
        require(!UnprojectCoords2D(a.get(), outside, world), "outside surface pick rejected");
        require(!ProjectCoords(a.get(), nullptr, screen), "null projection input rejected");
        require(!GetNearbyItems(a.get())[0] && !GetNearbyCharacters(a.get())[0], "no stale CPU interaction lists");
        first.fail_camera = true;
        screen[0] = 99; world[0] = 99;
        require(!ProjectCoords(a.get(), pos, screen) && screen[0] == 99, "failed projection commits no output");
        require(!UnprojectCoords3D(a.get(), screen, world) && world[0] == 99, "failed unprojection commits no output");
        require(!UnprojectCoords2D(a.get(), pick, world) && world[0] == 99, "failed surface pick commits no output");
        first.fail_camera = false; first.nonfinite_pick = true;
        require(!UnprojectCoords2D(a.get(), pick, world) && world[0] == 99, "nonfinite collision result rejected");
        draw(a.get(), 161);
        require(first.calls == 1 && GetSceneBackendError(a.get()), "invalid bridge bounds rejected before dispatch");
        pos[0] = std::numeric_limits<float>::infinity(); draw(a.get());
        require(first.calls == 1 && GetSceneBackendError(a.get()), "invalid metadata rejected before dispatch");
        pos[0] = -12;
        first.fail_render = true; draw(a.get());
        require(first.calls == 2 && std::strstr(GetSceneBackendError(a.get()), "shader/pass"), "backend failure retained, no hidden CPU fallback");
        require(!ProjectCoords(a.get(), pos, screen), "failed frame invalidates stale camera state");
        draw(b.get());
        require(second.calls == 1 && !GetSceneBackendError(b.get()), "other renderer unaffected by failure");
        first.fail_render = false; first.nonfinite_pick = false; draw(a.get());
        require(first.calls == 3 && !GetSceneBackendError(a.get()), "successful retry clears failure");
        SetSceneBackend(a.get(), nullptr);
        require(!GetSceneBackendError(a.get()), "detach resets status");
        draw(a.get());
        require(GetSceneBackendError(a.get()) && std::strstr(GetSceneBackendError(a.get()), "No GPU"),
                "missing backend fails explicitly without a software fallback");
        require(!ProjectCoords(a.get(), pos, screen), "detached backend has no stale camera");
        SetSceneBackend(a.get(), &second);
        require(!ProjectCoords(a.get(), pos, screen), "reattach requires a new frame");
        draw(a.get()); require(second.calls == 2, "reattach selects new backend");
        SetSceneBackend(a.get(), nullptr); SetSceneBackend(b.get(), nullptr);
        std::puts("SCENE_BACKEND_OK dispatch isolation camera picking bounds failure detach");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "SCENE_BACKEND_FAILURE %s\n", e.what()); return 1;
    }
}
