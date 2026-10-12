#include "games/asciicker/gpu_scene.h"
#include "games/asciicker/scene_camera.h"
#include "games/asciicker/terrain_geometry.h"
#include "games/asciicker/mesh_geometry.h"
#include "games/asciicker/picking.h"
#include "games/asciicker/terrain_changes.h"
#include "platform/graphics/device.h"
#include "sprite.h"
#include "game.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <chrono>
extern Character *player_head;
extern Game *game;

namespace port::asciicker {
namespace {
class GpuScene final : public engine::SceneBackend {
    graphics::Device &device_;
    Terrain *terrain_;
    World *world_;
    SceneCamera camera_;
    engine::SceneFrame frame_;
    engine::Geometry sprites_;
    engine::Geometry foreground_;
    bool reflection_ = false;
    std::vector<engine::GeometryInstance> draws_;
    std::vector<engine::GeometryInstance> reflected_draws_;
    engine::SurfaceTexture surface_;
    std::vector<TerrainTile> tiles_;
    int columns_ = 0, rows_ = 0;
    bool overflow_ = false;
    bool reported_ = false;
    engine::SceneTimings timings_;

    void emit_cell(int x, int y, float depth, engine::Cell cell) {
        if (x < 0 || y < 0 || x >= columns_ || y >= rows_)
            return;
        if (sprites_.vertices.size() + 4 > 65536) {
            overflow_ = true;
            return;
        }
        const float width = columns_ * 2 + 4, height = rows_ * 2 + 4;
        const float x0 = (2 * x + 2) * 2 / width - 1, x1 = (2 * x + 4) * 2 / width - 1;
        const float y0 = (2 * y + 2) * 2 / height - 1, y1 = (2 * y + 4) * 2 / height - 1;
        const std::array<float, 4> color{cell.fg / 255.0F, cell.bk / 255.0F, cell.gl / 255.0F, 128 / 255.0F};
        const auto first = static_cast<uint16_t>(sprites_.vertices.size());
        sprites_.vertices.insert(sprites_.vertices.end(), {{{x0, y0, depth}, color},
                                                           {{x1, y0, depth}, color},
                                                           {{x0, y1, depth}, color},
                                                           {{x1, y1, depth}, color}});
        for (int offset : {0, 1, 2, 2, 1, 3})
            sprites_.indices.push_back(first + offset);
    }
    void projectiles() {
        for (Character *character = player_head; character; character = character->next) {
            if (character->req.kind != SpriteReq::HUMAN)
                continue;
            const auto *human = static_cast<const Human *>(character);
            if (!human->shooting || frame_.timestamp_us < human->shoot_stamp)
                continue;
            const float dx = human->shoot_to[0] - human->shoot_from[0];
            const float dy = human->shoot_to[1] - human->shoot_from[1];
            const float dz = (human->shoot_to[2] - human->shoot_from[2]) / 16;
            const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (length < 1e-6F)
                continue;
            const float elapsed = (frame_.timestamp_us - human->shoot_stamp) / 1000000.0F;
            const float head = std::min(1.0F, elapsed * 1000 / length);
            const float tail = std::max(0.0F, (elapsed - 0.05F) * 1000 / length);
            engine::ScreenPoint first, last;
            const engine::Position a{(human->shoot_from[0] + tail * dx) / 4,
                                     (human->shoot_from[1] + tail * dy) / 4,
                                     human->shoot_from[2] + tail * dz * 16};
            const engine::Position b{(human->shoot_from[0] + head * dx) / 4,
                                     (human->shoot_from[1] + head * dy) / 4,
                                     human->shoot_from[2] + head * dz * 16};
            if (!camera_.project(a, first) || !camera_.project(b, last))
                continue;
            const int steps = std::min(256, std::max(std::abs(last.x - first.x), std::abs(last.y - first.y)));
            for (int i = 0; i <= steps; ++i) {
                const float t = steps ? static_cast<float>(i) / steps : 0;
                engine::Point3 pixel;
                const engine::Point3 world{a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]),
                                           (a[2] + t * (b[2] - a[2])) / 16};
                if (engine::project_point(camera_.matrix(), {columns_ * 2 + 4, rows_ * 2 + 4}, world, pixel))
                    emit_cell(static_cast<int>(std::lround(first.x + t * (last.x - first.x))),
                              static_cast<int>(std::lround(first.y + t * (last.y - first.y))), pixel[2],
                              {231, 255, 7, 0});
            }
        }
    }

    static void ignore_mesh(Mesh *, double[16], void *) {}
    static void sprite(Inst *inst, Sprite *sprite, float position[3], float yaw, int anim, int frame, int[4],
                       void *cookie) {
        auto &self = *static_cast<GpuScene *>(cookie);
        if (!sprite || !(GetInstFlags(inst) & INST_VISIBLE))
            return;
        if (self.reflection_ && sprite->projs < 2)
            return;
        if (anim < 0) {
            if (frame != Item::WORLD)
                return;
            anim = frame = 0;
        }
        if (anim >= sprite->anims || anim < 0 || sprite->angles < 1)
            return;
        if (anim >= 0)
            frame = AnimateSpriteInst(inst, self.frame_.timestamp_us);
        const auto &animation = sprite->anim[anim];
        if (animation.length < 1)
            return;
        frame = std::clamp(frame, 0, animation.length - 1);
        int angle =
            static_cast<int>(std::lround((yaw - self.frame_.camera.yaw_degrees) * sprite->angles / 360));
        angle = (angle % sprite->angles + sprite->angles) % sprite->angles;
        const int index = animation.frame_idx[frame + angle * animation.length +
                                              (self.reflection_ ? animation.length * sprite->angles : 0)];
        if (index < 0 || index >= sprite->frames)
            return;
        const auto &image = sprite->atlas[index];
        engine::ScreenPoint anchor;
        const float z = self.reflection_ ? 2 * self.frame_.water_height - position[2] : position[2];
        if (!self.camera_.project({position[0], position[1], z}, anchor))
            return;
        const int left = anchor.x - image.ref[0] / 2, bottom = anchor.y - image.ref[1] / 2;
        const float width = self.columns_ * 2 + 4, height = self.rows_ * 2 + 4;
        const auto *character = static_cast<const Character *>(GetInstSpriteData(inst));
        if (!self.reflection_ && character && (!game || character != &game->player) &&
            character->req.action != ACTION::DEAD && character->MAX_HP > 0) {
            engine::Point3 pixel;
            if (engine::project_point(self.camera_.matrix(), {self.columns_ * 2 + 4, self.rows_ * 2 + 4},
                                      {position[0], position[1], (position[2] + 8 + 10 * 12.316806F) / 16},
                                      pixel)) {
                const int filled = std::clamp(character->HP * 5 / character->MAX_HP, 0, 5);
                for (int x = 0; x < 5; ++x)
                    self.emit_cell(anchor.x + x - 2, anchor.y + 10, pixel[2],
                                   {static_cast<uint8_t>(character->clr ? 208 : 196),
                                    static_cast<uint8_t>(character->clr ? 130 : 124),
                                    static_cast<uint8_t>(x < filled ? 219 : 32), 0});
            }
        }
        for (int y = 0; y < image.height; ++y)
            for (int x = 0; x < image.width; ++x) {
                const int cx = left + x, cy = bottom + y;
                if (cx < 0 || cy < 0 || cx >= self.columns_ || cy >= self.rows_)
                    continue;
                const auto cell = image.cell[x + y * image.width];
                if ((cell.fg == 255 && cell.bk == 255) ||
                    ((cell.gl == 0 || cell.gl == 32) && cell.bk == 255) || (cell.gl == 219 && cell.fg == 255))
                    continue;
                if (self.sprites_.vertices.size() + 4 > 65536) {
                    self.overflow_ = true;
                    return;
                }
                const float elevation = z + 8 + (cell.spare + image.ref[2] * 0.5F) * 12.316806F;
                engine::Point3 pixel;
                if (!engine::project_point(self.camera_.matrix(),
                                           {static_cast<int>(width), static_cast<int>(height)},
                                           {position[0], position[1], elevation / 16}, pixel))
                    continue;
                if (self.reflection_ ? elevation <= self.frame_.water_height
                                     : elevation >= self.frame_.water_height)
                    self.emit_cell(cx, cy, pixel[2], cell);
            }
    }

  public:
    GpuScene(graphics::Device &device, Terrain *terrain, World *world)
        : device_(device), terrain_(terrain), world_(world) {}
    bool initialize(std::string &error) {
        TerrainGeometry terrain;
        MeshGeometry meshes;
        if (!collect_terrain_geometry(terrain_, terrain, error) ||
            !collect_mesh_geometry(world_, terrain.chunks.size(), meshes, error))
            return false;
        for (size_t i = 0; i < terrain.chunks.size(); ++i)
            draws_.push_back({i});
        draws_.insert(draws_.end(), meshes.instances.begin(), meshes.instances.end());
        for (auto &mesh : meshes.resources)
            terrain.chunks.push_back(std::move(mesh));
        engine::Geometry water;
        const std::array<float, 4> water_color{0.43F, 0.42F, 0.79F, 192 / 255.0F};
        constexpr float extent = 4096;
        for (const auto &position : {engine::Point3{-extent, -extent, 0}, engine::Point3{extent, -extent, 0},
                                     engine::Point3{-extent, extent, 0}, engine::Point3{extent, extent, 0}})
            water.vertices.push_back({position, water_color, {0, 0, 0, 2}});
        water.indices = {0, 1, 2, 2, 1, 3};
        draws_.push_back({terrain.chunks.size()});
        terrain.chunks.push_back(std::move(water));
        reflected_draws_.resize(draws_.size() - 1);
        if (!device_.set_surface(terrain.surface, error) || !device_.upload(terrain.chunks, error))
            return false;
        surface_ = std::move(terrain.surface);
        tiles_ = std::move(terrain.tiles);
        take_terrain_changes();
        return true;
    }
    bool render(const engine::SceneFrame &frame, engine::CellTarget target, std::string &error) override {
        using Clock = std::chrono::steady_clock;
        const auto begin = Clock::now();
        timings_ = {};
        if (!engine::valid_frame(frame, target) ||
            !camera_.configure(frame.camera, target.columns, target.rows)) {
            error = "Invalid GPU scene camera/target";
            return false;
        }
        frame_ = frame;
        columns_ = target.columns;
        rows_ = target.rows;
        const auto changes = take_terrain_changes();
        if (changes.count || changes.all) {
            for (size_t index = 0; index < tiles_.size(); ++index) {
                auto &tile = tiles_[index];
                const std::array<int, 2> coordinate{tile.x, tile.y};
                if (!changes.all &&
                    std::find(changes.patches.begin(), changes.patches.begin() + changes.count, coordinate) ==
                        changes.patches.begin() + changes.count)
                    continue;
                Patch *patch = GetTerrainPatch(terrain_, tile.x / 8, tile.y / 8);
                if (!patch) {
                    error = "Dirty terrain patch disappeared";
                    return false;
                }
                tile = snapshot_tile(patch, tile.x, tile.y);
                const int ax = static_cast<int>(index % 102) * 10, ay = static_cast<int>(index / 102) * 10;
                for (int y = 0; y < 10; ++y)
                    for (int x = 0; x < 10; ++x) {
                        const int source = std::clamp(y - 1, 0, 7) * 8 + std::clamp(x - 1, 0, 7);
                        auto *texel = surface_.rgba.data() + ((ay + y) * surface_.width + ax + x) * 4;
                        texel[0] = tile.visual[source] & 255;
                        texel[1] = ((tile.dark >> source) & 1) * 255;
                        texel[2] = tile.visual[source] >> 15;
                        texel[3] = 255;
                    }
                if (!changes.all && !device_.update_surface(surface_, ax, ay, 10, 10, error))
                    return false;
            }
            if (changes.all &&
                !device_.update_surface(surface_, 0, 0, surface_.width, surface_.height, error))
                return false;
        }
        sprites_.vertices.clear();
        sprites_.indices.clear();
        overflow_ = false;
        QueryWorldCB callbacks{ignore_mesh, sprite};
        QueryWorld(world_, 0, nullptr, &callbacks, this);
        projectiles();
        if (!reported_) {
            std::printf("GPU_SCENE_READY static_draws=%u sprite_cells=%u camera=%.2f,%.2f,%.2f\n",
                        static_cast<unsigned>(draws_.size()),
                        static_cast<unsigned>(sprites_.vertices.size() / 4), frame.camera.position[0],
                        frame.camera.position[1], frame.camera.position[2]);
            reported_ = true;
        }
        if (overflow_) {
            error = "GPU sprite transient budget exceeded";
            return false;
        }
        std::fill_n(target.data, static_cast<size_t>(target.columns) * target.rows,
                    engine::Cell{255, 255, 0, 0});
        const float water = frame.water_height / 16;
        auto mirror = engine::identity_matrix();
        mirror[10] = -1;
        mirror[14] = 2 * water;
        for (size_t i = 0; i < reflected_draws_.size(); ++i)
            reflected_draws_[i] = {draws_[i].geometry, engine::multiply_matrix(mirror, draws_[i].transform)};
        draws_.back().transform[14] = water;
        foreground_.vertices.swap(sprites_.vertices);
        foreground_.indices.swap(sprites_.indices);
        sprites_.vertices.clear();
        sprites_.indices.clear();
        reflection_ = true;
        QueryWorld(world_, 0, nullptr, &callbacks, this);
        reflection_ = false;
        if (overflow_) {
            error = "Reflected sprite transient budget exceeded";
            return false;
        }
        device_.set_pass({water, -1, true, 0, frame.light});
        const auto collected = Clock::now();
        timings_.collect_ms = std::chrono::duration<double, std::milli>(collected - begin).count();
        if (!device_.set_dynamic(sprites_, error))
            return false;
        if (!device_.render(camera_.matrix(), reflected_draws_, error) || !device_.capture_reflection(error))
            return false;
        const auto reflected = Clock::now();
        timings_.reflection_ms = std::chrono::duration<double, std::milli>(reflected - collected).count();
        foreground_.vertices.swap(sprites_.vertices);
        foreground_.indices.swap(sprites_.indices);
        device_.set_pass(
            {water, 1, true, static_cast<float>(frame.timestamp_us % 60000000) / 1000000, frame.light});
        const bool success =
            device_.set_dynamic(sprites_, error) && device_.render(camera_.matrix(), draws_, error);
        timings_.primary_ms = std::chrono::duration<double, std::milli>(Clock::now() - reflected).count();
        return success;
    }
    engine::SceneTimings timings() const override {
        return timings_;
    }
    bool project(const engine::Position &world, engine::ScreenPoint &screen) const override {
        return camera_.project(world, screen);
    }
    bool unproject(const engine::ScreenPoint &screen, engine::Position &world) const override {
        return camera_.unproject(screen, world);
    }
    bool pick_surface(int x, int y, engine::Position &world) const override {
        engine::CameraRay ray;
        engine::Point3 hit;
        if (!camera_.ray(x, y, ray) || !pick_world_surface(terrain_, world_, ray, hit))
            return false;
        world = {hit[0] * 4, hit[1] * 4, hit[2] * 16};
        return true;
    }
};
} // namespace
std::unique_ptr<engine::SceneBackend> create_gpu_scene(graphics::Device &device, Terrain *terrain,
                                                       World *world, std::string &error) {
    auto scene = std::make_unique<GpuScene>(device, terrain, world);
    if (!scene->initialize(error))
        return {};
    return scene;
}
} // namespace port::asciicker
