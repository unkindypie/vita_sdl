#pragma once
#include <array>
#include <cstddef>
namespace port::asciicker {
struct TerrainChanges {
    std::array<std::array<int, 2>, 256> patches{};
    size_t count = 0;
    bool all = false;
};
// Single game-thread resource invalidation queue, not simulation state.
void mark_terrain_changed(int x, int y);
TerrainChanges take_terrain_changes();
} // namespace port::asciicker
