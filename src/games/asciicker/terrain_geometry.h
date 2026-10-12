#pragma once
#include "engine/render/geometry.h"
#include "engine/render/ascii.h"
#include <array>
struct Terrain;
struct Patch;
namespace port::asciicker {
// Game-format metadata remains in the adapter, not engine or native backends.
struct TerrainTile {
    int x, y;
    std::array<uint16_t,25> heights;
    std::array<uint16_t,64> visual;
    uint16_t diagonals;
    uint64_t dark;
};
struct TerrainGeometry {
    std::vector<TerrainTile> tiles;
    std::vector<engine::Geometry> chunks;
    engine::SurfaceTexture surface;
};
TerrainTile snapshot_tile(Patch*, int x, int y);
engine::Geometry tile_geometry(const TerrainTile&);
bool collect_terrain_geometry(Terrain*, TerrainGeometry&, std::string& error);
}
