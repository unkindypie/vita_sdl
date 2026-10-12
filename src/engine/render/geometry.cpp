#include "engine/render/geometry.h"
#include <cmath>
#include <algorithm>

namespace engine {
Matrix normal_matrix(const Matrix &model) {
    Matrix result = identity_matrix();
    const std::array<float, 3> a{model[0], model[1], model[2]};
    const std::array<float, 3> b{model[4], model[5], model[6]};
    const std::array<float, 3> c{model[8], model[9], model[10]};
    auto cross = [](const auto &u, const auto &v) {
        return std::array<float, 3>{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                                    u[0] * v[1] - u[1] * v[0]};
    };
    const auto x = cross(b, c), y = cross(c, a), z = cross(a, b);
    const double determinant = static_cast<double>(a[0]) * x[0] + static_cast<double>(a[1]) * x[1] +
                               static_cast<double>(a[2]) * x[2];
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-20)
        return result;
    for (int row = 0; row < 3; ++row) {
        result[row] = static_cast<float>(x[row] / determinant);
        result[4 + row] = static_cast<float>(y[row] / determinant);
        result[8 + row] = static_cast<float>(z[row] / determinant);
    }
    return result;
}
Matrix identity_matrix() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
Matrix rotation_y(float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    return {c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1};
}
Matrix multiply_matrix(const Matrix &a, const Matrix &b) {
    Matrix product{};
    for (size_t column = 0; column < 4; ++column)
        for (size_t row = 0; row < 4; ++row)
            for (size_t k = 0; k < 4; ++k)
                product[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
    return product;
}
bool valid_instances(const std::vector<GeometryInstance> &instances, size_t count, std::string &error) {
    if (instances.size() > 8192) {
        error = "Too many geometry instances";
        return false;
    }
    for (const auto &instance : instances) {
        if (instance.geometry >= count) {
            error = "Geometry instance references missing resource";
            return false;
        }
        for (float value : instance.transform)
            if (!std::isfinite(value)) {
                error = "Nonfinite instance transform";
                return false;
            }
    }
    return true;
}
bool valid_geometry(const Geometry &geometry, std::string &error) {
    if (geometry.vertices.empty() || geometry.vertices.size() > 65536 || geometry.indices.empty() ||
        geometry.indices.size() % 3 != 0 || geometry.indices.size() > 3 * 65536) {
        error = "Geometry requires nonempty bounded U16 triangle batches";
        return false;
    }
    for (const auto &vertex : geometry.vertices) {
        for (float component : vertex.position)
            if (!std::isfinite(component)) {
                error = "Nonfinite geometry position";
                return false;
            }
        for (float component : vertex.color)
            if (!std::isfinite(component) || component < 0 || component > 1) {
                error = "Geometry colors must be finite normalized RGBA";
                return false;
            }
        for (size_t channel = 0; channel < vertex.surface.size(); ++channel) {
            const float component = vertex.surface[channel];
            const float minimum = channel < 3 && vertex.surface[3] == 3 ? -1.0F : 0.0F;
            if (!std::isfinite(component) || component < minimum || component > (channel == 3 ? 3 : 1)) {
                error = "Geometry requires bounded surface data and kind in [0,3]";
                return false;
            }
        }
        if (vertex.surface[3] == 3 && vertex.surface[0] * vertex.surface[0] +
                                              vertex.surface[1] * vertex.surface[1] +
                                              vertex.surface[2] * vertex.surface[2] <
                                          1e-12F) {
            error = "Lit mesh requires a nonzero normal";
            return false;
        }
    }
    for (uint16_t index : geometry.indices)
        if (index >= geometry.vertices.size()) {
            error = "Geometry index exceeds vertex bounds";
            return false;
        }
    error.clear();
    return true;
}
GeometryBounds geometry_bounds(const Geometry &geometry) {
    if (geometry.vertices.empty())
        return {{0, 0, 0}, {0, 0, 0}};
    GeometryBounds bounds{geometry.vertices.front().position, geometry.vertices.front().position};
    for (const auto &vertex : geometry.vertices)
        for (size_t axis = 0; axis < 3; ++axis) {
            bounds.minimum[axis] = std::min(bounds.minimum[axis], vertex.position[axis]);
            bounds.maximum[axis] = std::max(bounds.maximum[axis], vertex.position[axis]);
        }
    return bounds;
}
bool intersects_clip_volume(const GeometryBounds &bounds, const Matrix &clip) {
    // AABB support-function test in local space, not CPU vertex rasterization.
    for (size_t axis = 0; axis < 3; ++axis)
        for (int sign : {-1, 1}) {
            float furthest = clip[15] + sign * clip[12 + axis];
            for (size_t k = 0; k < 3; ++k) {
                const float coefficient = clip[k * 4 + 3] + sign * clip[k * 4 + axis];
                furthest += coefficient * (coefficient >= 0 ? bounds.maximum[k] : bounds.minimum[k]);
            }
            if (furthest < -1e-4f)
                return false;
        }
    return true;
}
bool gather_visible_geometry(const Matrix &camera, const std::vector<GeometryInstance> &instances,
                             const std::vector<GeometryBounds> &bounds, std::vector<GeometryDraw> &draws,
                             std::string &error) {
    if (!valid_instances(instances, bounds.size(), error))
        return false;
    for (float value : camera)
        if (!std::isfinite(value)) {
            error = "Nonfinite camera matrix";
            return false;
        }
    draws.clear();
    draws.reserve(instances.size());
    for (const auto &instance : instances) {
        const Matrix mvp = multiply_matrix(camera, instance.transform);
        for (float value : mvp)
            if (!std::isfinite(value)) {
                error = "Instance MVP overflow";
                return false;
            }
        if (intersects_clip_volume(bounds[instance.geometry], mvp))
            draws.push_back({instance.geometry, mvp, instance.transform});
    }
    return true;
}
Geometry depth_fixture() {
    return {{{{-0.65f, -0.55f, -0.35f}, {0.9f, 0.3f, 0.25f, 1}},
             {{0.65f, -0.55f, -0.35f}, {0.9f, 0.3f, 0.25f, 1}},
             {{0.0f, 0.65f, -0.35f}, {0.9f, 0.3f, 0.25f, 1}},
             {{-0.85f, -0.75f, 0.35f}, {0.25f, 0.6f, 0.9f, 1}},
             {{0.85f, -0.75f, 0.35f}, {0.25f, 0.6f, 0.9f, 1}},
             {{0.0f, 0.85f, 0.35f}, {0.25f, 0.6f, 0.9f, 1}}},
            {0, 1, 2, 3, 4, 5}};
}
bool pack_geometry(const std::vector<Geometry> &batches, PackedGeometry &packed, std::string &error) {
    constexpr size_t kMaximumBytes = 16 * 1024 * 1024;
    size_t vertices = 0, indices = 0;
    if (batches.empty() || batches.size() > 1024) {
        error = "Static geometry requires 1..1024 batches";
        return false;
    }
    for (const auto &batch : batches) {
        if (!valid_geometry(batch, error))
            return false;
        vertices += batch.vertices.size();
        indices += batch.indices.size();
        if (vertices * sizeof(GeometryVertex) + indices * sizeof(uint16_t) > kMaximumBytes) {
            error = "Static geometry exceeds 16 MiB upload budget";
            return false;
        }
    }
    PackedGeometry result;
    result.vertices.reserve(vertices);
    result.indices.reserve(indices);
    result.ranges.reserve(batches.size());
    for (const auto &batch : batches) {
        result.ranges.push_back(
            {result.vertices.size(), result.indices.size(), static_cast<uint32_t>(batch.indices.size())});
        result.vertices.insert(result.vertices.end(), batch.vertices.begin(), batch.vertices.end());
        result.indices.insert(result.indices.end(), batch.indices.begin(), batch.indices.end());
    }
    packed = std::move(result);
    error.clear();
    return true;
}
} // namespace engine
