#include "games/asciicker/scene_camera.h"
#include <cmath>

namespace port::asciicker {
bool SceneCamera::configure(const engine::CameraView &view, int columns, int rows) {
    if (columns < 1 || rows < 1 || !std::isfinite(view.zoom) || view.zoom <= 0)
        return false;
    viewport_ = {columns * 2 + 4, rows * 2 + 4};
    const float angle = view.yaw_degrees * 0.0174532925199433F;
    const float c = std::cos(angle), s = std::sin(angle);
    const float sx = 6 * view.zoom / viewport_.width;
    const float sy = 6 * view.zoom / viewport_.height;
    clip_ = {c * sx,
             -s * 0.5F * sy,
             0,
             0,
             s * sx,
             c * 0.5F * sy,
             0,
             0,
             0,
             0.8660254F * sy,
             -2.0F / 4096,
             0,
             0,
             0,
             0,
             1};
    const engine::Position center{view.position[0], view.position[1], view.position[2] / 16};
    for (int row = 0; row < 3; ++row)
        for (int axis = 0; axis < 3; ++axis)
            clip_[12 + row] -= clip_[axis * 4 + row] * center[axis];
    if (view.perspective) {
        const float focal = 2.0F * std::fmax(viewport_.width, viewport_.height);
        clip_[3] = -4 * s / focal;
        clip_[7] = 4 * c / focal;
        clip_[15] -= clip_[3] * center[0] + clip_[7] * center[1];
    }
    for (int axis = 0; axis < 4; ++axis) {
        clip_[axis * 4] += 4.0F * view.cell_shift[0] / viewport_.width * clip_[axis * 4 + 3];
        clip_[axis * 4 + 1] += 4.0F * view.cell_shift[1] / viewport_.height * clip_[axis * 4 + 3];
    }
    engine::Point3 pixel;
    if (!engine::invert_matrix(clip_, inverse_) || !engine::project_point(clip_, viewport_, center, pixel))
        return false;
    center_depth_ = pixel[2];
    return true;
}
bool SceneCamera::project(const engine::Position &position, engine::ScreenPoint &screen) const {
    engine::Point3 pixel;
    if (!engine::project_point(clip_, viewport_, {position[0], position[1], position[2] / 16}, pixel))
        return false;
    screen = {static_cast<int>(std::floor((pixel[0] - 1.5F) / 2)),
              static_cast<int>(std::floor((viewport_.height - 2.5F - pixel[1]) / 2)),
              static_cast<int>(std::lround(position[2] + 8))};
    return true;
}
bool SceneCamera::ray(int x, int y, engine::CameraRay &result) const {
    // Architectural perspective has an oblique depth mapping: its infinite
    // clip-plane endpoint can be behind the eye. Use two nearby finite planes
    // around the focus depth instead of assuming both clip endpoints are valid.
    engine::Point3 first, second;
    const float px = 2 * x + 2.5F, py = viewport_.height - 3.5F - 2 * y;
    if (!engine::unproject_point(inverse_, viewport_, {px, py, center_depth_ - 0.0001F}, first) ||
        !engine::unproject_point(inverse_, viewport_, {px, py, center_depth_ + 0.0001F}, second))
        return false;
    const float dx = second[0] - first[0], dy = second[1] - first[1], dz = second[2] - first[2];
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (length < 1e-7F)
        return false;
    result.direction = {dx / length, dy / length, dz / length};
    // Collision queries require a ray beginning above the entire world.
    const float back = dz < -1e-7F ? (4096 - first[2]) / (-result.direction[2]) : 4096;
    for (int i = 0; i < 3; ++i)
        result.origin[i] = first[i] - back * result.direction[i];
    return true;
}
bool SceneCamera::unproject(const engine::ScreenPoint &screen, engine::Position &position) const {
    engine::CameraRay camera_ray;
    if (!ray(screen.x, screen.y, camera_ray) || std::fabs(camera_ray.direction[2]) < 1e-7F)
        return false;
    const float z = (screen.depth - 8) / 16.0F;
    const float distance = (z - camera_ray.origin[2]) / camera_ray.direction[2];
    position = {4 * (camera_ray.origin[0] + distance * camera_ray.direction[0]),
                4 * (camera_ray.origin[1] + distance * camera_ray.direction[1]), z * 16};
    return std::isfinite(position[0]) && std::isfinite(position[1]);
}
} // namespace port::asciicker
