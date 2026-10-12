#include "engine/render/scene_backend.h"
#include <cmath>

namespace engine {
bool valid_frame(const SceneFrame& frame, const CellTarget& target) noexcept {
    if (!target.data || target.columns <= 0 || target.rows <= 0 ||
        static_cast<std::size_t>(target.columns) > target.capacity / static_cast<std::size_t>(target.rows))
        return false;
    for (float value : frame.camera.position) if (!std::isfinite(value)) return false;
    for (float value : frame.light) if (!std::isfinite(value)) return false;
    return std::isfinite(frame.camera.yaw_degrees) && std::isfinite(frame.camera.zoom) &&
           frame.camera.zoom > 0 && std::isfinite(frame.water_height);
}
}
