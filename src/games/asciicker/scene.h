#pragma once
// Asciicker-specific loading/ownership, not part of the reusable engine API.
#include <cstdint>
#include <string>
#include "game.h"
extern Game* game;
namespace port {
bool validate_assets(const std::string& root, const std::string& manifest, std::string& error);
bool load_scene(const std::string& root, uint64_t stamp, std::string& error);
void close_scene();
}
