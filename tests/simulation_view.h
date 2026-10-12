#pragma once
#include "engine/render/scene_backend.h"
#include "games/asciicker/scene_camera.h"
#include "games/asciicker/picking.h"
#include <algorithm>
extern Terrain *terrain;
extern World *world;

// Test double, not a software renderer: keeps camera/collision and independent
// UI observable without requiring GL in gameplay/ownership unit tests. Actual
// geometry, ASCII pixels and playable presentation have separate GPU tests.
class SimulationView final : public engine::SceneBackend {
    port::asciicker::SceneCamera camera_;

  public:
    unsigned calls = 0;
    bool render(const engine::SceneFrame &frame, engine::CellTarget target, std::string &) override {
        if (!engine::valid_frame(frame, target) ||
            !camera_.configure(frame.camera, target.columns, target.rows))
            return false;
        ++calls;
        std::fill_n(target.data, target.columns * target.rows, engine::Cell{255, 255, 0, 0});
        return true;
    }
    bool project(const engine::Position &position, engine::ScreenPoint &screen) const override {
        return camera_.project(position, screen);
    }
    bool unproject(const engine::ScreenPoint &screen, engine::Position &position) const override {
        return camera_.unproject(screen, position);
    }
    bool pick_surface(int x, int y, engine::Position &position) const override {
        engine::CameraRay ray;
        engine::Point3 hit;
        if (!camera_.ray(x, y, ray) || !port::asciicker::pick_world_surface(terrain, world, ray, hit))
            return false;
        position = {hit[0] * 4, hit[1] * 4, hit[2] * 16};
        return true;
    }
};
