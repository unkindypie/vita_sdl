#pragma once
#include "engine/render/ascii.h"
#include "platform/graphics/device.h"
namespace port {
bool check_ascii_resolve(graphics::DeviceFactory &, const engine::AsciiResources &,
                         const std::string &screenshot, std::string &error, bool mixed = false,
                         bool mesh = false);
}
