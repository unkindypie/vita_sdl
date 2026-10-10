#include "app/scene.h"
#include "enemygen.h"
#include "inventory.h"
#include "platform/engine_hooks.h"
#include "scripting/runtime.h"
#include "audio.h"
#include <cstdio>
#include <fstream>
extern char base_path[1024];
extern Terrain* terrain;
extern World* world;
extern Material mat[256];
extern Game* game;
namespace port {
namespace { bool sprites_loaded = false; }
bool validate_assets(const std::string& root, const std::string& manifest, std::string& error) {
    std::ifstream list(manifest);
    if (!list) { error = "Cannot open asset manifest: " + manifest; return false; }
    std::string path;
    int count = 0;
    while (std::getline(list, path)) {
        if (path.empty()) continue;
        if (path.find("..") != std::string::npos || path.front() == '/') {
            error = "Invalid manifest entry: " + path; return false;
        }
        std::ifstream file(root + "/" + path, std::ios::binary);
        if (!file || file.peek() == std::ifstream::traits_type::eof()) {
            error = "Missing/empty asset: " + root + "/" + path; return false;
        }
        ++count;
    }
    if (!count) { error = "Empty asset manifest"; return false; }
    std::printf("ASSETS_OK files=%d root=%s\n", count, root.c_str());
    return true;
}
bool load_scene(const std::string& root, uint64_t stamp, std::string& error) {
    if (game || terrain || world) { error = "Scene already loaded"; return false; }
    if (root.size() + 2 >= sizeof(base_path)) { error = "Asset path too long"; return false; }
    std::snprintf(base_path, sizeof(base_path), "%s/", root.c_str());
    FILE* file = std::fopen((root + "/a3d/game_map_y8.a3d").c_str(), "rb");
    if (!file) { error = "Cannot open game_map_y8.a3d"; return false; }
    log_stage("sprites.begin");
    LoadSprites();
    sprites_loaded = true;
    log_stage("sprites.ready");
    terrain = LoadTerrain(file);
    bool valid = terrain != nullptr;
    for (int i = 0; valid && i < 256; ++i)
        valid = std::fread(mat[i].shade, 1, sizeof(MatCell) * 64, file) == sizeof(MatCell) * 64;
    if (valid) world = LoadWorld(file, false);
    int mesh_count = 0;
    if (world) {
        for (Mesh* mesh = GetFirstMesh(world); mesh; mesh = GetNextMesh(mesh)) {
            char name[256] = {};
            GetMeshName(mesh, name, sizeof(name));
            if (!UpdateMesh(mesh, (root + "/meshes/" + name).c_str())) {
                error = "Cannot load mesh: " + std::string(name); valid = false; break;
            }
            ++mesh_count;
        }
        if (valid) LoadEnemyGens(file);
    }
    std::fclose(file);
    if (!valid || !world) {
        if (error.empty()) error = "Map terrain/material/world load failed";
        close_scene(); return false;
    }
    log_stage("meshes.ready");
    RebuildWorld(world, true);
    log_stage("world.rebuilt");
    game = CreateGame();
    log_stage("game.created");
    float pos[3] = {0, 15, 0};
    float light[4] = {1, 0, 1, 0.5F};
    InitGame(game, 55, pos, 45, 0, light, stamp);
    log_stage("game.initialized");
    game->main_menu = false;
    game->show_buts = true;
    if (!start_scripting(error) || !load_script(root + "/scripts/startup.js", error)) {
        close_scene(); return false;
    }
    std::printf("SCENE_READY map=game_map_y8.a3d patches=%d meshes=%d scripting=QuickJS audio=SDL network=offline\n",
                GetTerrainPatches(terrain), mesh_count);
    return true;
}
void close_scene() {
    FreeAudio();
    stop_scripting();
    if (game) { FreeGame(game); DeleteGame(game); game = nullptr; }
    if (world) { DeleteWorld(world); world = nullptr; }
    if (terrain) { DeleteTerrain(terrain); terrain = nullptr; }
    FreeEnemyGens();
    PurgeItemInstCache();
    if (sprites_loaded) { FreeSprites(); sprites_loaded = false; }
}
}
