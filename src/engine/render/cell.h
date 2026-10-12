#pragma once
#include <cstdint>
#include <type_traits>

namespace engine {
// Bottom-left cell origin. Font coverage itself has a top-left origin.
// spare is compositor/backend metadata, not an additional colour channel.
struct Cell {
    uint8_t fg, bk, gl, spare;
};
static_assert(sizeof(Cell) == 4 && std::is_standard_layout<Cell>::value,
              "Cell must retain the packed four-byte asset/presenter layout");
}
