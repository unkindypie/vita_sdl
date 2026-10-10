#pragma once
#include <SDL2/SDL.h>
namespace port {
int engine_key(SDL_Keycode key);
int engine_button(Uint8 button);
int16_t axis_value(int16_t value);
}
