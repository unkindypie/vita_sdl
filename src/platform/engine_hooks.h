#pragma once
namespace port {
void set_config_path(const char* path);
bool should_exit();
void reset_exit();
void log_stage(const char* stage);
}
