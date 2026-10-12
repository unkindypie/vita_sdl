#include "engine/render/camera.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace engine {
PresentationRect fit_viewport(Viewport available, Viewport design) {
    if (available.width < 1 || available.height < 1 || design.width < 1 || design.height < 1)
        return {};
    const auto wide = static_cast<int64_t>(available.width) * design.height;
    const auto tall = static_cast<int64_t>(available.height) * design.width;
    const int width = wide > tall ? std::max(1, static_cast<int>(tall / design.height)) : available.width;
    const int height = wide > tall ? available.height : std::max(1, static_cast<int>(wide / design.width));
    return {(available.width - width) / 2, (available.height - height) / 2, width, height};
}
namespace {
bool transform(const Matrix &matrix, const Point3 &input, Point3 &output) {
    double result[4]{};
    for (int row = 0; row < 4; ++row) {
        result[row] = matrix[12 + row];
        for (int column = 0; column < 3; ++column)
            result[row] += static_cast<double>(matrix[column * 4 + row]) * input[column];
    }
    if (!std::isfinite(result[3]) || result[3] <= 1e-12)
        return false;
    Point3 candidate;
    for (int row = 0; row < 3; ++row) {
        const double value = result[row] / result[3];
        if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
            return false;
        candidate[row] = static_cast<float>(value);
    }
    output = candidate;
    return true;
}
bool valid(Viewport viewport) {
    return viewport.width > 0 && viewport.height > 0;
}
} // namespace
bool invert_matrix(const Matrix &matrix, Matrix &inverse) {
    double augmented[4][8]{};
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column) {
            if (!std::isfinite(matrix[column * 4 + row]))
                return false;
            augmented[row][column] = matrix[column * 4 + row];
            augmented[row][column + 4] = row == column ? 1 : 0;
        }
    for (int column = 0; column < 4; ++column) {
        int pivot = column;
        for (int row = column + 1; row < 4; ++row)
            if (std::abs(augmented[row][column]) > std::abs(augmented[pivot][column]))
                pivot = row;
        if (std::abs(augmented[pivot][column]) < 1e-12)
            return false;
        for (int k = 0; k < 8; ++k)
            std::swap(augmented[pivot][k], augmented[column][k]);
        const double scale = augmented[column][column];
        for (double &value : augmented[column])
            value /= scale;
        for (int row = 0; row < 4; ++row)
            if (row != column) {
                const double factor = augmented[row][column];
                for (int k = 0; k < 8; ++k)
                    augmented[row][k] -= factor * augmented[column][k];
            }
    }
    Matrix candidate;
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column) {
            const double value = augmented[row][column + 4];
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
                return false;
            candidate[column * 4 + row] = static_cast<float>(value);
        }
    inverse = candidate;
    return true;
}
bool project_point(const Matrix &clip, Viewport viewport, const Point3 &world, Point3 &pixel) {
    if (!valid(viewport))
        return false;
    Point3 candidate;
    if (!transform(clip, world, candidate))
        return false;
    candidate[0] = (candidate[0] + 1) * 0.5f * viewport.width - 0.5f;
    candidate[1] = (1 - candidate[1]) * 0.5f * viewport.height - 0.5f;
    if (!std::isfinite(candidate[0]) || !std::isfinite(candidate[1]))
        return false;
    pixel = candidate;
    return true;
}
bool unproject_point(const Matrix &inverse, Viewport viewport, const Point3 &pixel, Point3 &world) {
    if (!valid(viewport))
        return false;
    const Point3 clip = {(pixel[0] + 0.5f) * 2 / viewport.width - 1,
                         1 - (pixel[1] + 0.5f) * 2 / viewport.height, pixel[2]};
    return transform(inverse, clip, world);
}
bool camera_ray(const Matrix &inverse, Viewport viewport, float x, float y, CameraRay &ray) {
    Point3 near, far;
    if (!unproject_point(inverse, viewport, {x, y, -1}, near) ||
        !unproject_point(inverse, viewport, {x, y, 1}, far))
        return false;
    double length_squared = 0;
    for (int i = 0; i < 3; ++i) {
        const double difference = static_cast<double>(far[i]) - near[i];
        length_squared += difference * difference;
    }
    if (!std::isfinite(length_squared) || length_squared < 1e-20)
        return false;
    CameraRay candidate;
    candidate.origin = near;
    const double inverse_length = 1 / std::sqrt(length_squared);
    for (int i = 0; i < 3; ++i)
        candidate.direction[i] = static_cast<float>((static_cast<double>(far[i]) - near[i]) * inverse_length);
    ray = candidate;
    return true;
}
} // namespace engine
