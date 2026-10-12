#pragma once
#include "engine/render/scene_backend.h"
#include <memory>
struct Terrain;
struct World;
namespace port::graphics { class Device; }
namespace port::asciicker {
// Device and world outlive the backend. Construction uploads immutable world
// resources; per-frame work only gathers visible animated sprite cells.
std::unique_ptr<engine::SceneBackend> create_gpu_scene(
    graphics::Device&, Terrain*, World*, std::string& error);
}
