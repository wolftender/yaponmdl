#pragma once
#include <cstdint>
#include <vector>
#include <span>

namespace gxt {

struct GxtImageBitmap {
    uint32_t width;
    uint32_t height;

    std::vector<uint8_t> rgba_plane;
};

auto LoadBitmaps(std::span<const uint8_t> buffer) -> std::vector<GxtImageBitmap>;

} // namespace gxt