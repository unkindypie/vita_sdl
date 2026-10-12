#pragma once
#include "engine/render/cell.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace engine {
using Position = std::array<float, 3>;
struct ScreenPoint { int x, y, depth; };
struct CameraView {
    Position position{};
    float yaw_degrees = 0;
    float zoom = 1;
    std::array<int, 2> cell_shift{};
    bool perspective = false;
};
struct SceneFrame {
    uint64_t timestamp_us = 0;
    CameraView camera;
    float water_height = 0;
    std::array<float, 4> light{1, 0, 1, 0.5F};
};
struct CellTarget {
    Cell* data = nullptr;
    int columns = 0, rows = 0;
    std::size_t capacity = 0;
};
// CPU wall time, including backend synchronization where applicable. These
// are not GPU timer queries and must not be summed with the enclosing scene.
struct SceneTimings {
    double collect_ms = 0;
    double reflection_ms = 0;
    double primary_ms = 0;
};
bool valid_frame(const SceneFrame& frame, const CellTarget& target) noexcept;

// Backend owns/caches its render resources, not gameplay or simulation. Frame
// and target are borrowed for this call; copying metadata is safe, retaining
// target.data is not. Rendering never advances scripts, AI or physics.
// A GPU backend prepares its offscreen scene and an independent cell overlay;
// it must NOT read back scene cells just to satisfy this legacy overlay bridge.
class SceneBackend {
public:
    virtual ~SceneBackend() = default;
    virtual bool render(const SceneFrame& frame, CellTarget target, std::string& error) = 0;
    // Coordinates use the game adapter's documented world units and bottom-left
    // cell coordinates. depth is a world-height value, not a normalized Z-buffer.
    virtual bool project(const Position& world, ScreenPoint& screen) const = 0;
    virtual bool unproject(const ScreenPoint& screen, Position& world) const = 0;
    // Implement via camera rays/collision, not per-frame GPU depth readback.
    virtual bool pick_surface(int cell_x, int cell_y, Position& world) const = 0;
    virtual SceneTimings timings() const { return {}; }
};
}
