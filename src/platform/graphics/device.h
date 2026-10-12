#pragma once
#include "engine/render/geometry.h"
#include "engine/render/ascii.h"
#include <memory>

namespace port::graphics {
struct ScenePass {
    float water_height = 0;
    float clip_sign = 0; // 0 disabled, +1 above plane, -1 below plane.
    bool dynamic = true;
    float time_seconds = 0;
    std::array<float, 4> light{1, 0, 1, 0.35F};
};
// Owns its context, shaders and offscreen color/depth targets. No SDL renderer
// state is borrowed. One static batch for the first graphics-ownership stage.
// Top-down tightly packed RGBA8 readback is diagnostic-only, never gameplay.
class Device {
  protected:
    std::vector<engine::GeometryInstance> default_draws_;
    std::vector<engine::GeometryBounds> bounds_;
    std::vector<engine::GeometryDraw> visible_;
    ScenePass pass_{};
    bool display_pacing_ = true;
    std::array<float, 4> clip_plane(const engine::Matrix &model) const {
        return {model[2] * pass_.clip_sign, model[6] * pass_.clip_sign, model[10] * pass_.clip_sign,
                (model[14] - pass_.water_height) * pass_.clip_sign};
    }
    void set_resources(const std::vector<engine::Geometry> &resources) {
        const size_t count = resources.size();
        bounds_.clear();
        bounds_.reserve(count);
        for (const auto &geometry : resources)
            bounds_.push_back(engine::geometry_bounds(geometry));
        default_draws_.clear();
        default_draws_.reserve(count);
        for (size_t i = 0; i < count; ++i)
            default_draws_.push_back({i});
    }

  public:
    virtual ~Device() = default;
    virtual bool upload(const std::vector<engine::Geometry> &, std::string &error) = 0;
    bool upload(const engine::Geometry &geometry, std::string &error) {
        return upload(std::vector<engine::Geometry>{geometry}, error);
    }
    bool render(const engine::Matrix &camera, std::string &error) {
        return render(camera, default_draws_, error);
    }
    virtual bool render(const engine::Matrix &, const std::vector<engine::GeometryInstance> &,
                        std::string &error) = 0;
    // GPU-only copy/resolve into the backend-owned window/display queue.
    virtual bool present(std::string &error) = 0;
    // Diagnostic finite benchmarks only; normal Vita play retains VBlank.
    void set_display_pacing(bool enabled) {
        display_pacing_ = enabled;
    }
    void set_pass(ScenePass pass) {
        pass_ = pass;
    }
    virtual bool capture_reflection(std::string &error) = 0;
    // Optional material-driven ASCII resolve; targets include a two-sample
    // border on each side. Upload immutable font/materials once at scene load.
    virtual bool set_ascii(const engine::AsciiResources &, std::string &error) = 0;
    virtual bool set_surface(const engine::SurfaceTexture &, std::string &error) = 0;
    virtual bool update_surface(const engine::SurfaceTexture &, int x, int y, int width, int height,
                                std::string &error) = 0;
    // Bounded transient clip-space geometry (sprites/projectiles), submitted
    // after static geometry against the same GPU depth. Empty clears it.
    virtual bool set_dynamic(const engine::Geometry &, std::string &error) = 0;
    // CPU-generated UI only. Cell origin remains bottom-left; 255 channels
    // preserve the GPU scene. No scene readback is permitted here.
    virtual bool set_overlay(const engine::Cell *, int columns, int rows, std::string &error) = 0;
    virtual bool readback(std::vector<uint8_t> &rgba, std::string &error) = 0;
    virtual bool readback_display(std::vector<uint8_t> &rgba, int &width, int &height,
                                  std::string &error) = 0;
    virtual const char *name() const = 0;
    size_t visible_draws() const {
        return visible_.size();
    }
};
class DeviceFactory {
  public:
    virtual ~DeviceFactory() = default;
    virtual std::unique_ptr<Device> create(int width, int height, std::string &error) = 0;
};
// Own the graphics runtime across context/target recreation; devices also keep
// it alive until their last GPU allocation has been released.
std::unique_ptr<DeviceFactory> create_device_factory(std::string &error);
} // namespace port::graphics
