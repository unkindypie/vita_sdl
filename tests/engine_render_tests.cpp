#include "engine/render/scene_backend.h"
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}
int main() {
    try {
        engine::Cell cells[4]{};
        const engine::SceneFrame good{};
        const engine::CellTarget target{cells, 2, 2, 4};
        require(engine::valid_frame(good, target), "valid standalone engine frame");
        require(!engine::valid_frame(good, {nullptr, 2, 2, 4}), "null cells rejected");
        require(!engine::valid_frame(good, {cells, 0, 2, 4}), "zero columns rejected");
        require(!engine::valid_frame(good, {cells, 2, -1, 4}), "negative rows rejected");
        require(!engine::valid_frame(good, {cells, 2, 2, 3}), "short target rejected");
        require(!engine::valid_frame(good, {cells, 2147483647, 2147483647, 4}), "large dimensions rejected without overflow");
        for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity()}) {
            for (int component = 0; component < 3; ++component) {
                auto frame = good;
                frame.camera.position[component] = invalid;
                require(!engine::valid_frame(frame, target), "nonfinite camera rejected");
            }
            for (int component = 0; component < 4; ++component) {
                auto frame = good;
                frame.light[component] = invalid;
                require(!engine::valid_frame(frame, target), "nonfinite light rejected");
            }
            auto frame = good; frame.water_height = invalid;
            require(!engine::valid_frame(frame, target), "nonfinite water rejected");
            frame = good; frame.camera.yaw_degrees = invalid;
            require(!engine::valid_frame(frame, target), "nonfinite yaw rejected");
            frame = good; frame.camera.zoom = invalid;
            require(!engine::valid_frame(frame, target), "nonfinite zoom rejected");
        }
        for (float invalid : {0.0F, -1.0F}) {
            auto frame = good; frame.camera.zoom = invalid;
            require(!engine::valid_frame(frame, target), "nonpositive zoom rejected");
        }
        auto negative = good;
        negative.camera.position = {-100, -20, -30};
        negative.camera.yaw_degrees = -45;
        negative.camera.cell_shift = {-10, 2};
        negative.water_height = -100;
        require(engine::valid_frame(negative, target), "negative world coordinates are valid");
        std::puts("ENGINE_RENDER_CONTRACT_OK no SDL/game/platform dependencies");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ENGINE_RENDER_CONTRACT_FAILURE %s\n", e.what()); return 1;
    }
}
