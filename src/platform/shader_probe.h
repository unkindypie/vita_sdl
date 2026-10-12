#pragma once
#include <string>
namespace port {
// Explicit, startup-only native Vita compiler check. Does not create a GPU
// context or claim that compiled programs render correctly on hardware.
bool probe_vita_shader_compiler(std::string& error);
}
