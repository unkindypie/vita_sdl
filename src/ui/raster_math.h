#pragma once

namespace port {
// Equivalent to (int)floor(value) for finite inputs whose floor fits in int,
// the existing renderer's coordinate/color domain. Preserve the input precision.
// Truncation plus a negative-fraction correction avoids newlib floor calls in
// ARM raster loops. Do not use this to validate untrusted/out-of-range numbers.
template<class Float> inline int floor_to_int(Float value) noexcept {
    const int truncated = static_cast<int>(value);
    return truncated - static_cast<int>(static_cast<Float>(truncated) > value);
}
}
