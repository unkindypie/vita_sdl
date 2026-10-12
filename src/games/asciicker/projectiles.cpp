#include "games/asciicker/projectiles.h"
#include "game.h"
#include "physics.h"
#include "fast_rand.h"
#include <cmath>
#include <cstdlib>
extern Character *player_head;
namespace port::asciicker {
void advance_projectiles(uint64_t timestamp) {
    for (Character *character = player_head; character; character = character->next) {
        if (character->req.kind != SpriteReq::HUMAN)
            continue;
        auto *human = static_cast<Human *>(character);
        if (!human->shooting)
            continue;
        if (timestamp < human->shoot_stamp)
            continue;
        if (timestamp - human->shoot_stamp > 3000000) {
            human->shooting = false;
            human->shoot_target = nullptr;
            continue;
        }
        const float dx = human->shoot_to[0] - human->shoot_from[0];
        const float dy = human->shoot_to[1] - human->shoot_from[1];
        const float dz = (human->shoot_to[2] - human->shoot_from[2]) / HEIGHT_SCALE;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        const double tail_time = (timestamp - human->shoot_stamp) / 1000000.0 - 0.05;
        if (tail_time * 1000 <= length)
            continue;
        if (auto *target = human->shoot_target) {
            const int hp = target->HP;
            target->HP -= std::rand() % 50;
            target->leak += (hp - target->HP) / 2;
            float radius = fast_rand() % 20 * 0.1f + 0.6f;
            if (hp > 0 && target->HP <= 0)
                radius = std::fmax(radius, 2.5f);
            const float dr = std::sqrt((fast_rand() & 0xfff) / static_cast<float>(0xfff));
            const float angle = (fast_rand() & 0xfff) * 6.28318530718f / 0xfff;
            float xy[] = {target->pos[0] + dr * std::cos(angle), target->pos[1] + dr * std::sin(angle)};
            PaintTerrain(xy, radius, 5);
            const float ix = target->pos[0] - human->pos[0], iy = target->pos[1] - human->pos[1];
            const float separation = std::hypot(ix, iy);
            if (separation > 1e-6f) {
                target->impulse[0] += ix / separation;
                target->impulse[1] += iy / separation;
            }
            if (target->HP <= 0) {
                if (target->req.mount != MOUNT::NONE) {
                    static_cast<Human *>(target)->SetMount(MOUNT::NONE);
                    target->HP = hp;
                } else {
                    target->dir = std::atan2(-iy, -ix) * 57.295779513f + 90;
                    SetPhysicsDir(static_cast<Physics *>(target->data), target->dir);
                    target->HP = 0;
                    target->SetActionFall(timestamp);
                }
            }
        }
        human->shooting = false;
        human->shoot_target = nullptr;
    }
}
} // namespace port::asciicker
