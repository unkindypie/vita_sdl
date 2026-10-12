#include "games/asciicker/picking.h"
#include "terrain.h"
#include "world.h"
#include <cmath>
#include <limits>
namespace port::asciicker {
bool pick_world_surface(Terrain *terrain, World *world, const engine::CameraRay &ray, engine::Point3 &hit) {
    double origin[3], direction[3];
    double length_squared = 0;
    for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(ray.origin[i]) || !std::isfinite(ray.direction[i]))
            return false;
        origin[i] = ray.origin[i] * (i == 2 ? HEIGHT_SCALE : 1);
        direction[i] = ray.direction[i] * (i == 2 ? HEIGHT_SCALE : 1);
        length_squared += direction[i] * direction[i];
    }
    if (length_squared < 1e-20)
        return false;
    double nearest = std::numeric_limits<double>::infinity();
    engine::Point3 candidate{};
    auto accept = [&](const double point[3]) {
        double distance = 0;
        for (int i = 0; i < 3; ++i)
            distance += (point[i] - origin[i]) * direction[i];
        distance /= length_squared;
        if (!std::isfinite(distance) || distance < 0 || distance >= nearest)
            return;
        nearest = distance;
        for (int i = 0; i < 3; ++i)
            candidate[i] = static_cast<float>(point[i] / (i == 2 ? HEIGHT_SCALE : 1));
    };
    double terrain_hit[4]{}, mesh_hit[3]{};
    if (terrain && HitTerrain(terrain, origin, direction, terrain_hit, nullptr, true))
        accept(terrain_hit);
    if (world && HitWorld(world, origin, direction, mesh_hit, nullptr, true, false, true, false))
        accept(mesh_hit);
    if (!std::isfinite(nearest))
        return false;
    hit = candidate;
    return true;
}
} // namespace port::asciicker
