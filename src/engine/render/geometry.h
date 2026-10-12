#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace engine {
// Column-major matrices; clip depth is [-1,1], y points up on both backends.
using Matrix = std::array<float, 16>;
struct GeometryVertex {
    std::array<float, 3> position;
    std::array<float, 4> color;
    // Optional material UV, computed light, and kind (0 direct/1 LUT/2 water).
    std::array<float, 4> surface{};
};
struct Geometry {
    std::vector<GeometryVertex> vertices;
    std::vector<uint16_t> indices;
};
struct GeometryRange {
    size_t vertex_offset, index_offset;
    uint32_t index_count;
};
struct PackedGeometry {
    std::vector<GeometryVertex> vertices;
    std::vector<uint16_t> indices;
    std::vector<GeometryRange> ranges;
};
// One bounded static upload, with local U16 indices for each draw range.
bool pack_geometry(const std::vector<Geometry> &batches, PackedGeometry &, std::string &error);
Matrix identity_matrix();
Matrix rotation_y(float radians);
Matrix multiply_matrix(const Matrix &, const Matrix &);
struct GeometryInstance {
    size_t geometry;
    Matrix transform = identity_matrix();
};
bool valid_instances(const std::vector<GeometryInstance> &, size_t resource_count, std::string &error);
struct GeometryBounds {
    std::array<float, 3> minimum, maximum;
};
struct GeometryDraw {
    size_t geometry;
    Matrix mvp;
    Matrix model = identity_matrix();
};
GeometryBounds geometry_bounds(const Geometry &);
bool intersects_clip_volume(const GeometryBounds &, const Matrix &);
bool gather_visible_geometry(const Matrix &, const std::vector<GeometryInstance> &,
                             const std::vector<GeometryBounds> &, std::vector<GeometryDraw> &,
                             std::string &error);
bool valid_geometry(const Geometry &geometry, std::string &error);
// Small fixture, deliberately submitted near-first/far-last to detect missing
// depth testing. Not a substitute for the Asciicker terrain adapter.
Geometry depth_fixture();
} // namespace engine
