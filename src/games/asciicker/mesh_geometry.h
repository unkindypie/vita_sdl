#pragma once
#include "engine/render/geometry.h"
struct World;
namespace port::asciicker {
struct MeshGeometry {
    std::vector<engine::Geometry> resources;
    std::vector<engine::GeometryInstance> instances;
    size_t triangles = 0;
    size_t line_segments = 0; // Authored lines expanded into crossed GPU ribbons.
};
bool collect_mesh_geometry(World*, size_t resource_offset, MeshGeometry&, std::string& error);
}
