#pragma once
#include <cstdint>
#include <vector>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

namespace gxt {

struct GxtImageBitmap {
    uint32_t width = 0;
    uint32_t height = 0;

    glm::fvec2 uv_scale = glm::fvec2{1.0f, 1.0f};
    glm::fvec2 uv_offset = glm::fvec2{0.0f, 0.0f};

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