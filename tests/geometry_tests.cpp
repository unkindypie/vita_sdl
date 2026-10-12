#include "engine/render/geometry.h"
#include "engine/render/camera.h"
#include <cmath>
#include <cstdio>
#include <limits>
int main() {
    int failures = 0;
    auto check = [&](bool ok, const char *name) {
        if (!ok) {
            std::fprintf(stderr, "FAIL %s\n", name);
            ++failures;
        }
    };
    std::string error;
    auto normal_model = engine::identity_matrix();
    normal_model[0] = 2;
    normal_model[5] = 4;
    normal_model[10] = -0.5F;
    normal_model[12] = 123;
    auto normal = engine::normal_matrix(normal_model);
    check(std::abs(normal[0] - 0.5F) < 1e-6F && std::abs(normal[5] - 0.25F) < 1e-6F &&
              std::abs(normal[10] + 2) < 1e-6F && std::abs(normal[12]) < 1e-6F,
          "normal inverse transpose preserves scale/reflection and ignores translation");
    normal_model[0] = 0;
    normal = engine::normal_matrix(normal_model);
    const auto normal_identity = engine::identity_matrix();
    for (size_t i = 0; i < normal.size(); ++i)
        check(std::abs(normal[i] - normal_identity[i]) < 1e-6F, "singular normal transform fallback");
    const auto tall_window = engine::fit_viewport({960, 1000});
    check(tall_window.x == 0 && tall_window.y == 228 && tall_window.width == 960 && tall_window.height == 544,
          "desktop letterbox preserves native aspect");
    const auto wide_window = engine::fit_viewport({1920, 544});
    check(wide_window.x == 480 && wide_window.y == 0 && wide_window.width == 960 && wide_window.height == 544,
          "desktop pillarbox preserves native aspect");
    check(engine::fit_viewport({0, 544}).width == 0 && engine::fit_viewport({1, 1}).height == 1,
          "invalid and tiny viewport bounds");
    auto geometry = engine::depth_fixture();
    check(engine::valid_geometry(geometry, error), "fixture bounds");
    auto lit_geometry = geometry;
    lit_geometry.vertices[0].surface = {-1, 0, 0, 3};
    check(engine::valid_geometry(lit_geometry, error), "signed lit normal accepted");
    lit_geometry.vertices[0].surface = {0, 0, 0, 3};
    check(!engine::valid_geometry(lit_geometry, error), "zero lit normal rejected");
    auto bad = geometry;
    bad.indices[0] = 65535;
    check(!engine::valid_geometry(bad, error), "invalid index");
    bad = geometry;
    bad.indices.pop_back();
    check(!engine::valid_geometry(bad, error), "incomplete triangle");
    bad = geometry;
    bad.vertices[0].position[0] = std::numeric_limits<float>::infinity();
    check(!engine::valid_geometry(bad, error), "nonfinite position");
    bad = geometry;
    bad.vertices[0].color[1] = -1;
    check(!engine::valid_geometry(bad, error), "unnormalized color");
    bad = geometry;
    bad.vertices[0].surface[0] = std::numeric_limits<float>::quiet_NaN();
    check(!engine::valid_geometry(bad, error), "nonfinite material UV");
    bad = geometry;
    bad.vertices.resize(65537);
    check(!engine::valid_geometry(bad, error), "U16 batch limit");
    check(!engine::valid_geometry({}, error), "empty batch");
    engine::PackedGeometry packed;
    check(engine::pack_geometry({geometry, geometry}, packed, error), "multi-batch packing");
    check(packed.ranges.size() == 2 && packed.ranges[1].vertex_offset == 6 &&
              packed.ranges[1].index_offset == 6 && packed.ranges[1].index_count == 6 &&
              packed.indices[6] == 0,
          "local U16 indices and offsets");
    check(!engine::pack_geometry({}, packed, error) && packed.ranges.size() == 2,
          "invalid upload preserves prior packed data");
    check(engine::valid_instances({{1}}, 2, error), "valid shared geometry instance");
    check(!engine::valid_instances({{2}}, 2, error), "invalid shared geometry resource");
    const auto identity = engine::identity_matrix(), zero = engine::rotation_y(0);
    const engine::GeometryBounds bounds{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
    check(engine::intersects_clip_volume(bounds, identity), "visible bounds");
    auto moved = identity;
    moved[12] = 2;
    check(!engine::intersects_clip_volume(bounds, moved), "offscreen bounds");
    moved[12] = 1.5f;
    check(engine::intersects_clip_volume(bounds, moved), "touching clip boundary retained");
    moved = identity;
    moved[14] = -2;
    check(!engine::intersects_clip_volume(bounds, moved), "near-plane culling");
    moved[14] = 2;
    check(!engine::intersects_clip_volume(bounds, moved), "far-plane culling");
    std::vector<engine::GeometryDraw> draws;
    auto model = identity;
    model[12] = 4;
    check(engine::gather_visible_geometry(identity, {{0}, {0, model}}, {bounds}, draws, error) &&
              draws.size() == 1,
          "shared resource visibility");
    model[0] = std::numeric_limits<float>::max();
    auto camera = identity;
    camera[0] = std::numeric_limits<float>::max();
    check(!engine::gather_visible_geometry(camera, {{0, model}}, {bounds}, draws, error),
          "matrix overflow rejected");
    for (size_t i = 0; i < 16; ++i)
        check(std::abs(identity[i] - zero[i]) < 1e-6f, "zero rotation");
    const auto quarter = engine::rotation_y(1.57079632679f);
    const auto combined = engine::multiply_matrix(quarter, identity);
    for (size_t i = 0; i < 16; ++i)
        check(std::abs(combined[i] - quarter[i]) < 1e-6f, "instance MVP multiplication");
    check(std::abs(quarter[0]) < 1e-6f && std::abs(quarter[2] + 1) < 1e-6f &&
              std::abs(quarter[8] - 1) < 1e-6f,
          "column major rotation");
    engine::Matrix inverse;
    check(engine::invert_matrix(identity, inverse), "identity camera inverse");
    engine::Point3 screen, restored;
    check(engine::project_point(identity, {244, 140}, {0, 0, 0}, screen) &&
              std::abs(screen[0] - 121.5f) < 1e-5f && std::abs(screen[1] - 69.5f) < 1e-5f,
          "pixel-center projection");
    check(engine::unproject_point(inverse, {244, 140}, screen, restored) && std::abs(restored[0]) < 1e-5f,
          "pixel-center unprojection");
    engine::CameraRay ray;
    check(engine::camera_ray(inverse, {244, 140}, 121.5f, 69.5f, ray) &&
              std::abs(ray.origin[2] + 1) < 1e-5f && std::abs(ray.direction[2] - 1) < 1e-5f,
          "orthographic ray");
    // Finite perspective camera, near=1/far=100, right-handed -Z view.
    const engine::Matrix perspective = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -101.0f / 99, -1, 0, 0, -200.0f / 99,
                                        0};
    check(engine::invert_matrix(perspective, inverse), "perspective inverse");
    for (const engine::Point3 point : {engine::Point3{0, 0, -5}, engine::Point3{1, -2, -10}}) {
        check(engine::project_point(perspective, {960, 544}, point, screen) &&
                  engine::unproject_point(inverse, {960, 544}, screen, restored),
              "perspective project roundtrip");
        for (int i = 0; i < 3; ++i)
            check(std::abs(restored[i] - point[i]) < 1e-4f, "perspective world tolerance");
    }
    check(engine::camera_ray(inverse, {960, 544}, 479.5f, 271.5f, ray) &&
              std::abs(ray.direction[2] + 1) < 1e-5f,
          "perspective ray direction");
    check(!engine::project_point(perspective, {960, 544}, {0, 0, 1}, screen),
          "behind-camera projection rejected");
    const auto prior_inverse = inverse;
    check(!engine::invert_matrix({}, inverse) && inverse == prior_inverse,
          "singular camera preserves inverse");
    check(!engine::camera_ray(inverse, {0, 544}, 0, 0, ray), "invalid camera viewport");
    return failures ? 1 : 0;
}
