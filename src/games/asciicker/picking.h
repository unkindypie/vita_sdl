#pragma once
#include "engine/render/camera.h"
struct Terrain;
struct World;
namespace port::asciicker {
// Engine coordinates use normalized elevation (upstream height / 16).
// World collision, not raster depth. Sprites are deliberately excluded from
// surface picking; game-owned character/item selection is a separate query.
bool pick_world_surface(Terrain *, World *, const engine::CameraRay &, engine::Point3 &hit);
} // namespace port::asciicker
