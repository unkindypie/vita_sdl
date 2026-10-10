#include "platform/input.h"
#include "platform.h"
namespace port {
int engine_touch_contact(int slot) { return slot >= 0 && slot < 3 ? slot + 1 : -1; }
int engine_key(SDL_Keycode key) {
    if (key >= SDLK_a && key <= SDLK_z) return A3D_A + static_cast<int>(key - SDLK_a);
    switch (key) {
        case SDLK_UP: return A3D_UP;
        case SDLK_DOWN: return A3D_DOWN;
        case SDLK_LEFT: return A3D_LEFT;
        case SDLK_RIGHT: return A3D_RIGHT;
        case SDLK_SPACE: return A3D_SPACE;
        case SDLK_RETURN: return A3D_ENTER;
        case SDLK_BACKSPACE: return A3D_BACKSPACE;
        case SDLK_TAB: return A3D_TAB;
        case SDLK_F1: return A3D_F1;
        case SDLK_DELETE: return A3D_DELETE;
        case SDLK_ESCAPE: return A3D_ESCAPE;
        case SDLK_LSHIFT: return A3D_LSHIFT;
        case SDLK_RSHIFT: return A3D_RSHIFT;
        default: return A3D_NONE;
    }
}
int engine_button(Uint8 button) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_A: return 0;
        case SDL_CONTROLLER_BUTTON_B: return 1;
        case SDL_CONTROLLER_BUTTON_X: return 2;
        case SDL_CONTROLLER_BUTTON_Y: return 3;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return -1;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return -1;
        case SDL_CONTROLLER_BUTTON_BACK: return 6;
        case SDL_CONTROLLER_BUTTON_START: return 9;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return 11;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return 12;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return 13;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return 14;
        default: return -1;
    }
}
int16_t axis_value(int16_t value) { return value > -6000 && value < 6000 ? 0 : value; }
}
