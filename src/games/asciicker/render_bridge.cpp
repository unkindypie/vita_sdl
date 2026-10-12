#include "render.h"
#include "engine/render/scene_backend.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

// Compatibility boundary for upstream gameplay/UI. Scene rasterization belongs
// entirely to the GPU backend; this object retains only camera/interaction state.
struct Renderer {
    engine::SceneBackend *backend = nullptr;
    bool ready = false;
    int columns = 0, rows = 0;
    std::string error;
    std::array<Item *, 10> items{};
    std::array<Inst *, 4> characters{};
};
Renderer *CreateRenderer(uint64_t) {
    return new Renderer;
}
void DeleteRenderer(Renderer *renderer) {
    delete renderer;
}
Item **GetNearbyItems(Renderer *renderer) {
    return renderer->items.data();
}
Inst **GetNearbyCharacters(Renderer *renderer) {
    return renderer->characters.data();
}
void SetNearbyInteractions(Renderer *renderer, Item *const *items, int item_count, Inst *const *characters,
                           int character_count) {
    renderer->items.fill(nullptr);
    renderer->characters.fill(nullptr);
    if (items)
        std::copy_n(items, std::clamp(item_count, 0, 9), renderer->items.begin());
    if (characters)
        std::copy_n(characters, std::clamp(character_count, 0, 3), renderer->characters.begin());
}
void SetSceneBackend(Renderer *renderer, engine::SceneBackend *backend) {
    renderer->backend = backend;
    renderer->ready = false;
    renderer->columns = renderer->rows = 0;
    renderer->error.clear();
    renderer->items.fill(nullptr);
    renderer->characters.fill(nullptr);
}
const char *GetSceneBackendError(const Renderer *renderer) {
    return renderer->error.empty() ? nullptr : renderer->error.c_str();
}
void Render(Renderer *renderer, uint64_t stamp, Terrain *, World *, float water, float zoom, float yaw,
            const float pos[3], const float light[4], int width, int height, AnsiCell *cells, Inst *,
            const int shift[2], bool perspective) {
    renderer->ready = false;
    renderer->error.clear();
    if (!renderer->backend) {
        renderer->error = "No GPU scene backend attached";
        return;
    }
    if (!pos || !light || !shift || width < 1 || height < 1 || width > 160 || height > 160) {
        renderer->error = "Invalid Asciicker scene frame bounds/input";
        return;
    }
    const engine::SceneFrame frame{stamp,
                                   {{pos[0], pos[1], pos[2]}, yaw, zoom, {shift[0], shift[1]}, perspective},
                                   water,
                                   {light[0], light[1], light[2], light[3]}};
    const engine::CellTarget target{cells, width, height, static_cast<size_t>(width) * height};
    if (!engine::valid_frame(frame, target)) {
        renderer->error = "Invalid scene frame/target";
        return;
    }
    if (!renderer->backend->render(frame, target, renderer->error)) {
        if (renderer->error.empty())
            renderer->error = "Scene backend failed";
        return;
    }
    renderer->ready = true;
    renderer->columns = width;
    renderer->rows = height;
}
bool ProjectCoords(Renderer *renderer, const float pos[3], int view[3]) {
    engine::ScreenPoint point{};
    if (!renderer->ready || !renderer->backend || !pos || !view || !std::isfinite(pos[0]) ||
        !std::isfinite(pos[1]) || !std::isfinite(pos[2]) ||
        !renderer->backend->project({pos[0], pos[1], pos[2]}, point))
        return false;
    view[0] = point.x;
    view[1] = point.y;
    view[2] = point.depth;
    return true;
}
bool UnprojectCoords2D(Renderer *renderer, const int xy[2], float pos[3]) {
    engine::Position result{};
    if (!renderer->ready || !renderer->backend || !xy || !pos || xy[0] < 0 || xy[1] < 0 ||
        xy[0] >= renderer->columns || xy[1] >= renderer->rows ||
        !renderer->backend->pick_surface(xy[0], xy[1], result) || !std::isfinite(result[0]) ||
        !std::isfinite(result[1]) || !std::isfinite(result[2]))
        return false;
    std::copy(result.begin(), result.end(), pos);
    return true;
}
bool UnprojectCoords3D(Renderer *renderer, const int xyz[3], float pos[3]) {
    engine::Position result{};
    if (!renderer->ready || !renderer->backend || !xyz || !pos ||
        !renderer->backend->unproject({xyz[0], xyz[1], xyz[2]}, result) || !std::isfinite(result[0]) ||
        !std::isfinite(result[1]) || !std::isfinite(result[2]))
        return false;
    std::copy(result.begin(), result.end(), pos);
    return true;
}
