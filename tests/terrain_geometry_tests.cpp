#include "games/asciicker/terrain_geometry.h"
#include "games/asciicker/mesh_geometry.h"
#include "games/asciicker/ascii_resources.h"
#include "games/asciicker/picking.h"
#include "games/asciicker/scene_camera.h"
#include "games/asciicker/terrain_changes.h"
#include "ui/cells.h"
#include "render.h"
#include "terrain.h"
#include <cmath>
#include <cstdio>
#include <memory>
extern Material mat[256];

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    int failures = 0;
    auto check = [&](bool condition, const char *name) {
        if (!condition) {
            std::fprintf(stderr, "FAIL %s\n", name);
            ++failures;
        }
    };
    port::asciicker::TerrainTile tile{};
    port::asciicker::SceneCamera camera;
    engine::CameraView camera_view;
    camera_view.position = {0,15,247};
    camera_view.yaw_degrees = 45;
    for (bool perspective : {false,true}) {
        camera_view.perspective = perspective;
        check(camera.configure(camera_view,120,68), "game camera invertible");
        engine::ScreenPoint screen;
        check(camera.project(camera_view.position,screen), "game camera projects center");
        check(screen.x == 60 && screen.y == 34, "game camera center cell");
        engine::Point3 below;
        check(engine::project_point(camera.matrix(),{244,140},
                  {camera_view.position[0],camera_view.position[1],camera_view.position[2]/16-10},below) &&
              below[2] > -1 && below[2] < 1,
              "geometry below camera focus is not clipped by far plane");
        engine::Position restored;
        check(camera.unproject(screen,restored), "game camera height-plane unprojection");
        check(std::abs(restored[0]/4-camera_view.position[0]) < 1 &&
              std::abs(restored[1]/4-camera_view.position[1]) < 1 &&
              std::abs(restored[2]-camera_view.position[2]) < 1e-4F,
              "game camera expanded shooting units round trip");
        engine::CameraRay ray;
        check(camera.ray(screen.x,screen.y,ray), "game camera independent picking ray");
    }
    check(!camera.configure(camera_view,0,68), "invalid game camera rejected");
    std::vector<uint8_t> overlay;
    const engine::Cell overlay_cells[] = {{231,255,65,0},{255,255,0,0},{16,17,66,0},{18,19,67,0}};
    check(engine::pack_overlay(overlay_cells,2,2,overlay), "overlay packing");
    check(overlay[0] == 16 && overlay[1] == 17 && overlay[2] == 66 && overlay[3] == 255,
          "overlay bottom-up cells become top-down texels");
    check(overlay[128*4] == 231 && overlay[128*4+1] == 255 && overlay[128*4+2] == 65,
          "overlay transparent channel preserved");
    check(overlay[128*4+7] == 0, "transparent overlay cell inactive");
    port::asciicker::take_terrain_changes();
    port::asciicker::mark_terrain_changed(-8,16);
    port::asciicker::mark_terrain_changed(-8,16);
    auto changes = port::asciicker::take_terrain_changes();
    check(changes.count == 1 && changes.patches[0][0] == -8 && !changes.all,
          "terrain invalidation deduplicates negative patch coordinates");
    check(port::asciicker::take_terrain_changes().count == 0,"terrain changes consumed once");
    for (int i = 0; i < 257; ++i) port::asciicker::mark_terrain_changed(i*8,0);
    changes = port::asciicker::take_terrain_changes();
    check(changes.count == 256 && changes.all,"terrain change overflow requests complete refresh");
    tile.x = -8;
    tile.y = 16;
    for (size_t i = 0; i < tile.heights.size(); ++i)
        tile.heights[i] = static_cast<uint16_t>(i * 16);
    auto geometry = port::asciicker::tile_geometry(tile);
    check(geometry.vertices.size() == 25 && geometry.indices.size() == 96, "tile size");
    check(std::abs(geometry.vertices.front().position[0] + 8) < 1e-6f &&
              std::abs(geometry.vertices.back().position[1] - 24) < 1e-6f &&
              std::abs(geometry.vertices.back().position[2] - 24) < 1e-6f,
          "negative origin and height units");
    check(geometry.indices[0] == 1 && geometry.indices[1] == 6 && geometry.indices[2] == 0, "diagonal zero");
    tile.diagonals = 1;
    geometry = port::asciicker::tile_geometry(tile);
    check(geometry.indices[0] == 0 && geometry.indices[1] == 1 && geometry.indices[2] == 5, "diagonal one");
    const std::string path = std::string(argv[1]) + "/a3d/game_map_y8.a3d";
    FILE *file = std::fopen(path.c_str(), "rb");
    if (!file)
        return 1;
    std::unique_ptr<Terrain, decltype(&DeleteTerrain)> terrain(LoadTerrain(file), DeleteTerrain);
    std::fclose(file);
    port::asciicker::TerrainGeometry snapshot;
    std::string error;
    check(port::asciicker::collect_terrain_geometry(terrain.get(), snapshot, error), error.c_str());
    check(snapshot.tiles.size() == 4876, "all real-map patches collected");
    engine::Point3 picked;
    check(port::asciicker::pick_world_surface(terrain.get(), nullptr, {{0, 15, 4096}, {0, 0, -1}}, picked),
          "terrain picking before renderer creation");
    double ray_origin[3] = {0, 15, 65536}, ray_direction[3] = {0, 0, -1}, collision[4]{};
    check(HitTerrain(terrain.get(), ray_origin, ray_direction, collision, nullptr, true) &&
              std::abs(picked[2] - collision[2] / 16) < 1e-4,
          "picking normalized elevation");
    check(!port::asciicker::pick_world_surface(terrain.get(), nullptr, {{0, 15, 4096}, {0, 0, 0}}, picked),
          "degenerate picking ray rejected");
    check(snapshot.surface.valid(), "bounded terrain surface atlas");
    for (size_t t = 0; t < snapshot.tiles.size(); ++t) {
        const auto &source = snapshot.tiles[t];
        const int ax = static_cast<int>(t % 102) * 10, ay = static_cast<int>(t / 102) * 10;
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 10; ++x) {
                const int index = std::clamp(y - 1, 0, 7) * 8 + std::clamp(x - 1, 0, 7);
                const auto *texel =
                    snapshot.surface.rgba.data() + ((ay + y) * snapshot.surface.width + ax + x) * 4;
                check(texel[0] == (source.visual[index] & 255) &&
                          texel[1] == ((source.dark >> index) & 1) * 255 &&
                          texel[2] == (source.visual[index] >> 15),
                      "surface material/dark/elevation and gutters");
            }
    }
    size_t vertices = 0, indices = 0;
    for (const auto &chunk : snapshot.chunks) {
        check(chunk.vertices.size() <= 1600, "bounded spatial chunk");
        vertices += chunk.vertices.size();
        indices += chunk.indices.size();
    }
    check(vertices == snapshot.tiles.size() * 25 && indices == snapshot.tiles.size() * 96,
          "no missing terrain geometry");
    // Compare copied metadata to actual map storage, including negative patches.
    for (const auto &source : snapshot.tiles) {
        Patch *patch = GetTerrainPatch(terrain.get(), source.x / 8, source.y / 8);
        check(patch != nullptr, "patch coordinates map back to source");
        if (!patch)
            continue;
        const auto copy = port::asciicker::snapshot_tile(patch, source.x, source.y);
        check(copy.heights == source.heights && copy.visual == source.visual &&
                  copy.diagonals == source.diagonals && copy.dark == source.dark,
              "lossless map metadata");
    }
    std::printf("TERRAIN_GEOMETRY tiles=%zu chunks=%zu vertices=%zu triangles=%zu\n", snapshot.tiles.size(),
                snapshot.chunks.size(), vertices, indices / 3);
    std::unique_ptr<World, decltype(&DeleteWorld)> world(CreateWorld(), DeleteWorld);
    const std::string mesh_path = std::string(argv[1]) + "/meshes/skull.akm";
    Mesh *mesh = LoadMesh(world.get(), mesh_path.c_str());
    check(mesh != nullptr, "load real mesh");
    if (mesh) {
        double transform[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 10, -20, 160, 1};
        CreateInst(mesh, INST_VISIBLE, transform, "first", -1);
        transform[12] = -40;
        CreateInst(mesh, INST_VISIBLE, transform, "second", -1);
        RebuildWorld(world.get(), true);
        port::asciicker::MeshGeometry meshes;
        check(port::asciicker::collect_mesh_geometry(world.get(), snapshot.chunks.size(), meshes, error),
              error.c_str());
        check(meshes.resources.size() == 1 && meshes.instances.size() == 2, "shared mesh uploaded once");
        if (meshes.instances.size() == 2) {
            for (const auto &instance : meshes.instances)
                check(instance.geometry == snapshot.chunks.size() &&
                          std::abs(instance.transform[14] - 10) < 1e-6f &&
                          std::abs(instance.transform[10] - 1.0f / 16) < 1e-6f,
                      "mesh instance offsets/elevation normalization");
        }
        check(meshes.triangles + meshes.line_segments == static_cast<size_t>(GetMeshFaces(mesh)),
              "all mesh primitives accounted");
    }
    // Exhaustive LUT addressing, CP437 atlas orientation, and failed-load
    // transactionality. Materials are synthetic so every byte is observable.
    for (int m = 0; m < 256; ++m)
        for (int e = 0; e < 4; ++e)
            for (int s = 0; s < 16; ++s) {
                auto &cell = mat[m].shade[e][s];
                cell.gl = static_cast<uint8_t>(m);
                cell.flags = static_cast<uint8_t>(e * 16 + s);
                cell.fg[0] = 255;
                cell.fg[1] = s * 17;
                cell.fg[2] = e * 85;
                cell.bg[0] = 0;
                cell.bg[1] = e * 85;
                cell.bg[2] = s * 17;
            }
    engine::AsciiResources ascii;
    const std::string font_path = std::string(argv[1]) + "/fonts/cp437_8x8.png";
    check(port::asciicker::collect_ascii_resources(font_path, ascii, error), error.c_str());
    for (int m = 0; m < 256; ++m)
        for (int e = 0; e < 4; ++e)
            for (int s = 0; s < 16; ++s) {
                const auto *pixel = ascii.materials.data() + (m * 128 + s * 8 + e * 2) * 4;
                check(pixel[3] == m && pixel[7] == e * 16 + s, "material/glyph/flags address");
                const uint32_t fg = port::display_color(
                    static_cast<uint8_t>(16 + 180 + ((s * 17 + 25) / 51) * 6 + (e * 85 + 25) / 51));
                check(pixel[0] == ((fg >> 16) & 255) && pixel[1] == ((fg >> 8) & 255) &&
                          pixel[2] == (fg & 255),
                      "shared pastel color quantization");
            }
    port::Font font;
    check(port::load_font(font_path, font, error), error.c_str());
    for (int g = 0; g < 256; ++g)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                check(ascii.font[(((g / 16) * 8 + y) * 128 + (g % 16) * 8 + x) * 4 + 3] ==
                          font[g * 64 + y * 8 + x],
                      "CP437 atlas row orientation");
    const auto prior = ascii.materials;
    check(!port::asciicker::collect_ascii_resources("missing-font.png", ascii, error) &&
              ascii.materials == prior,
          "failed ASCII load preserves resources");
    return failures ? 1 : 0;
}
