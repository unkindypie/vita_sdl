#pragma once
#include "ui/cells.h"
#include <SDL2/SDL.h>
#include <memory>
#include <vector>

namespace port {
// GPU glyph/background expansion; scene rasterization remains independent.
// Renderer must outlive this object. Buffers/atlas are allocated once.
class GpuPresenter {
public:
    bool initialize(SDL_Renderer* renderer, const Font& font, int columns, int rows, std::string& error);
    bool prepare(const AnsiCell* cells, std::string& error);
    bool draw(std::string& error);
private:
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> atlas_{nullptr, SDL_DestroyTexture};
    std::vector<SDL_Vertex> vertices_;
    std::vector<int> indices_;
    int columns_ = 0, rows_ = 0;
    bool prepared_ = false;
};
}
