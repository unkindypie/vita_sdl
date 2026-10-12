#include "games/asciicker/terrain_geometry.h"
#include "terrain.h"
#include <algorithm>
#include <map>
#include <utility>
#include <cmath>

namespace port::asciicker {
namespace {
constexpr int kChunkWidth = 8 * VISUAL_CELLS;
int chunk_coordinate(int coordinate) {
    const int quotient = coordinate / kChunkWidth;
    return quotient - (coordinate % kChunkWidth < 0 ? 1 : 0);
}
void collect(Patch *patch, int x, int y, int, void *data) {
    static_cast<TerrainGeometry *>(data)->tiles.push_back(snapshot_tile(patch, x, y));
}
} // namespace
TerrainTile snapshot_tile(Patch *patch, int x, int y) {
    static_assert(HEIGHT_CELLS == 4 && VISUAL_CELLS == 8 && HEIGHT_SCALE == 16,
                  "Update the game adapter when upstream terrain topology changes");
    TerrainTile tile{};
    tile.x = x;
    tile.y = y;
    std::copy_n(GetTerrainHeightMap(patch), tile.heights.size(), tile.heights.begin());
    std::copy_n(GetTerrainVisualMap(patch), tile.visual.size(), tile.visual.begin());
    tile.diagonals = GetTerrainDiag(patch);
    tile.dark = GetTerrainDark(patch);
    return tile;
}
engine::Geometry tile_geometry(const TerrainTile &tile) {
    engine::Geometry geometry;
    geometry.vertices.reserve(25);
    geometry.indices.reserve(96);
    for (int y = 0; y <= HEIGHT_CELLS; ++y)
        for (int x = 0; x <= HEIGHT_CELLS; ++x) {
            const float height = static_cast<float>(tile.heights[y * 5 + x]) / HEIGHT_SCALE;
            // Diagnostic height color only; material/dark maps are retained verbatim
            // for the later material-driven ASCII shader, not discarded/baked away.
            const float shade = std::min(height / 64.0f, 1.0f);
            geometry.vertices.push_back(
                {{static_cast<float>(tile.x + x * 2), static_cast<float>(tile.y + y * 2), height},
                 {0.28f + 0.4f * shade, 0.5f + 0.25f * shade, 0.3f + 0.3f * shade, 1}});
        }
    for (int y = 0; y < HEIGHT_CELLS; ++y)
        for (int x = 0; x < HEIGHT_CELLS; ++x) {
            const uint16_t a = y * 5 + x, b = a + 1, c = a + 5, d = c + 1;
            if ((tile.diagonals >> (y * 4 + x)) & 1)
                geometry.indices.insert(geometry.indices.end(), {a, b, c, d, c, b});
            else
                geometry.indices.insert(geometry.indices.end(), {b, d, a, c, a, d});
        }
    return geometry;
}
bool collect_terrain_geometry(Terrain *terrain, TerrainGeometry &output, std::string &error) {
    if (!terrain) {
        error = "Cannot collect null terrain";
        return false;
    }
    TerrainGeometry result;
    result.tiles.reserve(GetTerrainPatches(terrain));
    QueryTerrain(terrain, 0, nullptr, 0, collect, &result);
    if (result.tiles.empty()) {
        error = "Terrain contains no patches";
        return false;
    }
    // Stable spatial grouping handles negative coordinates without truncation
    // toward zero. At most 64 tiles / 1600 vertices per local-U16 draw batch.
    std::map<std::pair<int, int>, engine::Geometry> chunks;
    constexpr int kAtlasWidth = 1024, kAtlasHeight = 512, kTilesPerRow = 102;
    if (result.tiles.size() > kTilesPerRow * 51) {
        error = "Terrain surface atlas capacity exceeded";
        return false;
    }
    result.surface.width = kAtlasWidth;
    result.surface.height = kAtlasHeight;
    result.surface.rgba.resize(kAtlasWidth * kAtlasHeight * 4);
    size_t tile_index = 0;
    for (const auto &tile : result.tiles) {
        auto geometry = tile_geometry(tile);
        const int atlas_x = static_cast<int>(tile_index % kTilesPerRow) * 10,
                  atlas_y = static_cast<int>(tile_index / kTilesPerRow) * 10;
        // Duplicate a texel around each tile so nearest sampling at seams can
        // never borrow an unrelated patch's material. No per-frame CPU texture.
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 10; ++x) {
                const int source = std::clamp(y - 1, 0, 7) * 8 + std::clamp(x - 1, 0, 7);
                auto *data = result.surface.rgba.data() + ((atlas_y + y) * kAtlasWidth + atlas_x + x) * 4;
                data[0] = tile.visual[source] & 255;
                data[1] = ((tile.dark >> source) & 1) * 255;
                data[2] = tile.visual[source] >> 15;
                data[3] = 255;
            }
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < 5; ++x) {
                auto &vertex = geometry.vertices[y * 5 + x];
                const int x0 = std::max(0, x - 1), x1 = std::min(4, x + 1), y0 = std::max(0, y - 1),
                          y1 = std::min(4, y + 1);
                const float dx = (tile.heights[y * 5 + x1] - tile.heights[y * 5 + x0]) /
                                 static_cast<float>(HEIGHT_SCALE * 2 * (x1 - x0));
                const float dy = (tile.heights[y1 * 5 + x] - tile.heights[y0 * 5 + x]) /
                                 static_cast<float>(HEIGHT_SCALE * 2 * (y1 - y0));
                const float inverse = 1 / std::sqrt(dx * dx + dy * dy + 1);
                vertex.color = {(-dx * inverse + 1) * 0.5f, (-dy * inverse + 1) * 0.5f, (inverse + 1) * 0.5f,
                                1};
                vertex.surface = {(atlas_x + 1 + x * 2) / static_cast<float>(kAtlasWidth),
                                  (atlas_y + 1 + y * 2) / static_cast<float>(kAtlasHeight), 0, 1};
            }
        ++tile_index;
        auto &chunk = chunks[{chunk_coordinate(tile.x), chunk_coordinate(tile.y)}];
        const uint16_t base = static_cast<uint16_t>(chunk.vertices.size());
        chunk.vertices.insert(chunk.vertices.end(), geometry.vertices.begin(), geometry.vertices.end());
        for (uint16_t index : geometry.indices)
            chunk.indices.push_back(base + index);
    }
    for (auto &entry : chunks)
        result.chunks.push_back(std::move(entry.second));
    for (const auto &chunk : result.chunks)
        if (!engine::valid_geometry(chunk, error))
            return false;
    output = std::move(result);
    return true;
}
} // namespace port::asciicker
