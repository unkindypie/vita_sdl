#include "games/asciicker/interactions.h"
#include "game.h"
#include <array>
#include <algorithm>

namespace port::asciicker {
namespace {
template <class T, size_t N> struct Nearest {
    std::array<T *, N> values{};
    std::array<float, N> distances{};
    int count = 0;
    void add(T *value, float distance) {
        int index = 0;
        while (index < count && distances[index] <= distance)
            ++index;
        if (index == static_cast<int>(N))
            return;
        for (int move = std::min(count, static_cast<int>(N) - 1); move > index; --move) {
            values[move] = values[move - 1];
            distances[move] = distances[move - 1];
        }
        values[index] = value;
        distances[index] = distance;
        count = std::min(count + 1, static_cast<int>(N));
    }
};
struct Query {
    const float *position;
    Inst* player;
    Nearest<Item, 9> items;
    Nearest<Inst, 3> characters;
    static void mesh(Mesh *, double[16], void *) {}
    static void sprite(Inst *inst, Sprite *, float position[3], float, int anim, int frame, int reps[4],
                       void *cookie) {
        auto &query = *static_cast<Query *>(cookie);
        if(inst==query.player)return;
        const float dx = query.position[0] - position[0], dy = query.position[1] - position[1];
        const float dz = (query.position[2] + 3 * HEIGHT_SCALE - position[2]) / HEIGHT_SCALE;
        const float distance = dx * dx + dy * dy + dz * dz;
        if (anim < 0) {
            if (frame == Item::WORLD && distance < 20)
                query.items.add(reinterpret_cast<Item *>(reps), distance);
            return;
        }
        if (distance < 100)
            query.characters.add(inst, distance);
        auto *character = static_cast<Character *>(GetInstSpriteData(inst));
        if (!character || character->req.action != ACTION::DEAD || distance >= 20)
            return;
        ItemOwner *owner = character->req.kind == SpriteReq::HUMAN
                               ? static_cast<ItemOwner *>(static_cast<NPC_Human *>(character))
                               : static_cast<ItemOwner *>(static_cast<NPC_Creature *>(character));
        for (int i = 0; i < owner->items; ++i)
            query.items.add(owner->has[i].item, distance);
    }
};
} // namespace
void update_interactions(Renderer *renderer, World *world, const float position[3], Inst* player) {
    Query query{position, player, {}, {}};
    QueryWorldCB callbacks{Query::mesh, Query::sprite};
    QueryWorld(world, 0, nullptr, &callbacks, &query);
    SetNearbyInteractions(renderer, query.items.values.data(), query.items.count,
                          query.characters.values.data(), query.characters.count);
}
} // namespace port::asciicker
