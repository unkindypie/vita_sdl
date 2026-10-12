#pragma once
#include "engine/render/camera.h"
#include "engine/render/scene_backend.h"

namespace port::asciicker {
// Game positions use raw elevation; GPU geometry uses elevation / 16.
class SceneCamera {
    engine::Matrix clip_ = engine::identity_matrix();
    engine::Matrix inverse_ = engine::identity_matrix();
    engine::Viewport viewport_{};
    float center_depth_ = 0;

  public:
    bool configure(const engine::CameraView &, int columns, int rows);
    const engine::Matrix &matrix() const {
        return clip_;
    }
    bool project(const engine::Position &, engine::ScreenPoint &) const;
    // Upstream shooting expects expanded x/y (world coordinates * 4).
    bool unproject(const engine::ScreenPoint &, engine::Position &) const;
    bool ray(int cell_x, int cell_y, engine::CameraRay &) const;
};
} // namespace port::asciicker
