#include <SDL2/SDL.h>

#include <cstdio>

#ifdef __vita__
constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 544;
#else
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
#endif

int main() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Vita SDL2 Starter", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kWindowWidth, kWindowHeight, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    float x = 64.0F;
    float velocity = 180.0F;
    Uint64 previous = SDL_GetPerformanceCounter();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) ||
                (event.type == SDL_CONTROLLERBUTTONDOWN &&
                 event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK)) {
                running = false;
            }
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        const float delta = static_cast<float>(now - previous) /
                            static_cast<float>(SDL_GetPerformanceFrequency());
        previous = now;

        x += velocity * delta;
        if (x < 0.0F || x > kWindowWidth - 128.0F) {
            velocity = -velocity;
            x += velocity * delta;
        }

        SDL_SetRenderDrawColor(renderer, 20, 26, 45, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 66, 185, 131, 255);
        const SDL_Rect player = {static_cast<int>(x), kWindowHeight / 2 - 64, 128, 128};
        SDL_RenderFillRect(renderer, &player);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
