#pragma once
#include "engine/render/geometry.h"

namespace engine {
using Point3 = std::array<float, 3>;
struct Viewport {
    int width = 0, height = 0;
};
struct PresentationRect {
    int x = 0, y = 0, width = 0, height = 0;
};
// Top-left origin, integer viewport with the design aspect ratio preserved.
PresentationRect fit_viewport(Viewport available, Viewport design = {960, 544});
struct CameraRay {
    Point3 origin{}, direction{};
};
// Pixel coordinates are top-down, at pixel centers; depth is GL clip [-1,1].
// The functions never consult a render target or advance simulation.
bool invert_matrix(const Matrix &, Matrix &inverse);
bool project_point(const Matrix &clip, Viewport, const Point3 &, Point3 &pixel);
bool unproject_point(const Matrix &inverse, Viewport, const Point3 &pixel, Point3 &world);
bool camera_ray(const Matrix &inverse, Viewport, float x, float y, CameraRay &);
} // namespace engine
