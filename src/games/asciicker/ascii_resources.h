#pragma once
#include "engine/render/ascii.h"
#include <string>
namespace port::asciicker {
bool collect_ascii_resources(const std::string &font_path, engine::AsciiResources &, std::string &error);
}
