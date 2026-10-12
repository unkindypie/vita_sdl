#include "games/asciicker/mesh_geometry.h"
#include "world.h"
#include "terrain.h"
#include <map>
#include <utility>
#include <cmath>
namespace port::asciicker {
namespace {
struct Builder {
    engine::Geometry geometry;
    size_t lines = 0;
    size_t triangles = 0;
    bool overflow = false;
};
void face(float coords[9], uint8_t colors[12], uint32_t visual, void *cookie) {
    auto &builder = *static_cast<Builder *>(cookie);
    if (visual & (uint32_t{1} << 31)) {
        ++builder.lines;
        if (builder.geometry.vertices.size() > 65536 - 8) {
            builder.overflow = true;
            return;
        }
        const std::array<float, 3> direction{coords[3] - coords[0], coords[4] - coords[1],
                                             coords[5] - coords[2]};
        const float length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] +
                                       direction[2] * direction[2]);
        if (length < 1e-7F)
            return;
        std::array<float, 3> side{-direction[1], direction[0], 0};
        const float horizontal = std::hypot(side[0], side[1]);
        if (horizontal < 1e-7F)
            side = {1, 0, 0};
        else {
            side[0] /= horizontal;
            side[1] /= horizontal;
        }
        const std::array<float, 3> other{(direction[1] * side[2] - direction[2] * side[1]) / length,
                                         (direction[2] * side[0] - direction[0] * side[2]) / length,
                                         (direction[0] * side[1] - direction[1] * side[0]) / length};
        // Crossed narrow ribbons survive every view direction without a
        // per-frame CPU silhouette pass. They are depth-tested ASCII geometry.
        for (const auto &offset : {side, other}) {
            const auto first = static_cast<uint16_t>(builder.geometry.vertices.size());
            for (int endpoint : {0, 1})
                for (int sign : {-1, 1}) {
                    std::array<float, 3> position;
                    for (int axis = 0; axis < 3; ++axis)
                        position[axis] = coords[endpoint * 3 + axis] + offset[axis] * sign * 0.16F;
                    builder.geometry.vertices.push_back({position, {0.12F, 0.14F, 0.18F, 1}});
                }
            for (int index : {0, 1, 2, 2, 1, 3})
                builder.geometry.indices.push_back(first + index);
        }
        return;
    }
    ++builder.triangles;
    if (builder.geometry.vertices.size() > 65536 - 3) {
        builder.overflow = true;
        return;
    }
    const float ax = coords[3] - coords[0], ay = coords[4] - coords[1], az = coords[5] - coords[2];
    const float bx = coords[6] - coords[0], by = coords[7] - coords[1], bz = coords[8] - coords[2];
    std::array<float, 3> normal{ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx};
    const float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (length > 1e-12F)
        for (auto &value : normal)
            value /= length;
    else
        normal = {0, 0, 1};
    for (size_t i = 0; i < 3; ++i) {
        builder.geometry.indices.push_back(static_cast<uint16_t>(builder.geometry.vertices.size()));
        // Local mesh units are preserved; the instance matrix normalizes world
        // elevation after its authored transform. Alpha is not mesh opacity in
        // upstream: its scene rasterizer consumes only RGB here.
        builder.geometry.vertices.push_back(
            {{coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2]},
             {colors[i * 4] / 255.0f, colors[i * 4 + 1] / 255.0f, colors[i * 4 + 2] / 255.0f, 1},
             {normal[0], normal[1], normal[2], 3}});
    }
}
struct Collector {
    MeshGeometry result;
    std::map<Mesh *, size_t> ids;
    size_t offset;
    std::string error;
};
void instance(Mesh *mesh, double transform[16], void *cookie) {
    auto &collector = *static_cast<Collector *>(cookie);
    if (!collector.error.empty())
        return;
    auto found = collector.ids.find(mesh);
    if (found == collector.ids.end()) {
        Builder builder;
        QueryMesh(mesh, face, &builder);
        if (builder.overflow) {
            collector.error = "Mesh exceeds U16 batch budget; split the asset";
            return;
        }
        if (!engine::valid_geometry(builder.geometry, collector.error))
            return;
        const size_t index = collector.result.resources.size();
        found = collector.ids.emplace(mesh, index).first;
        collector.result.triangles += builder.triangles;
        collector.result.line_segments += builder.lines;
        collector.result.resources.push_back(std::move(builder.geometry));
    }
    engine::Matrix model{};
    for (size_t i = 0; i < 16; ++i)
        model[i] = static_cast<float>(transform[i]);
    // S * authored model: scale the z output row, including translation.
    for (size_t column = 0; column < 4; ++column)
        model[column * 4 + 2] /= HEIGHT_SCALE;
    collector.result.instances.push_back({collector.offset + found->second, model});
}
} // namespace
bool collect_mesh_geometry(World *world, size_t offset, MeshGeometry &output, std::string &error) {
    if (!world || offset > 1024) {
        error = "Invalid mesh world/resource offset";
        return false;
    }
    Collector collector{{}, {}, offset, {}};
    QueryWorldCB callbacks{};
    callbacks.mesh_cb = instance;
    callbacks.sprite_cb = [](Inst *, Sprite *, float *, float, int, int, int *, void *) {};
    QueryWorld(world, 0, nullptr, &callbacks, &collector);
    if (!collector.error.empty()) {
        error = collector.error;
        return false;
    }
    if (collector.result.resources.size() + offset > 1024 ||
        !engine::valid_instances(collector.result.instances, offset + collector.result.resources.size(),
                                 error)) {
        if (error.empty())
            error = "Too many mesh resources";
        return false;
    }
    output = std::move(collector.result);
    return true;
}
} // namespace port::asciicker
