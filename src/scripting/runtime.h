#pragma once
#include <cstdint>
#include <string>
namespace port {
struct ScriptStats {
    uint64_t calls = 0, errors = 0, microseconds = 0;
    int64_t memory_bytes = 0;
};
bool start_scripting(std::string& error);
void stop_scripting();
bool eval_script(const std::string& source, bool root, std::string& result);
bool load_script(const std::string& path, std::string& error);
ScriptStats script_stats();
uint64_t script_time_us();
}
