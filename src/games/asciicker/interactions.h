#pragma once
struct Renderer;
struct World;
struct Inst;
namespace port::asciicker {
void update_interactions(Renderer *, World *, const float position[3], Inst* player);
}
