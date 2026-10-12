#pragma once
#include <string>
namespace port::asciicker {
bool run_world_fixture(const std::string& assets, int frames,
                         const std::string& screenshot, std::string& error);
}
