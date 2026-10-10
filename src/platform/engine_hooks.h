#pragma once
namespace port {
void set_config_path(const char* path);
bool should_exit();
void reset_exit();
void request_new_game();
bool take_new_game_request();
void log_stage(const char* stage);
}
