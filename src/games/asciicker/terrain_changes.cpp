#include "games/asciicker/terrain_changes.h"
#include <algorithm>
namespace port::asciicker {
namespace {
TerrainChanges pending;
}
void mark_terrain_changed(int x, int y) {
    const std::array<int, 2> patch{x, y};
    if (std::find(pending.patches.begin(), pending.patches.begin() + pending.count, patch) !=
        pending.patches.begin() + pending.count)
        return;
    if (pending.count == pending.patches.size()) {
        pending.all = true;
        return;
    }
    pending.patches[pending.count++] = patch;
}
TerrainChanges take_terrain_changes() {
    const auto changes = pending;
    pending = {};
    return changes;
}
} // namespace port::asciicker
