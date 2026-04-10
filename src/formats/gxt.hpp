#pragma once
#include <cstdint>
#include <vector>
#include <span>
#include <string_view>

namespace gxt {

struct GxtImageBitmap {
    uint32_t width;
    uint32_t height;

    std::vector<uint8_t> rgba_plane;
};

class GxtLogger {
public:
    virtual ~GxtLogger() = default;
    virtual auto log(std::string_view log_message) const -> void = 0;
};

auto LoadBitmaps(std::span<const uint8_t> buffer, const GxtLogger *logger = nullptr) -> std::vector<GxtImageBitmap>;
auto CheckHeader(std::span<const uint8_t> buffer, const GxtLogger *logger = nullptr) -> bool;

} // namespace gxt