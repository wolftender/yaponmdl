#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <glm/glm.hpp>

namespace gxx {

struct GxxVertex {
    glm::fvec3 position;
    glm::fvec3 color;
    glm::fvec2 uv;
};

struct GxxMesh {
    std::vector<GxxVertex> vertices;
    std::vector<uint32_t> indices;
};

struct GxxTexture {
    uint32_t id;
    std::string name;
};

struct GxxBone {
    std::string name;
};

struct GxxAnimation {
    std::string name;
    uint32_t num_frames;
    float framerate;
    float frameloop;
};

struct GxxModel {
    std::vector<GxxTexture> textures;
    std::vector<GxxBone> bones;
    std::vector<GxxMesh> mesh;
    std::vector<GxxAnimation> animations;
};

class GxxLogger {
public:
    virtual ~GxxLogger() = default;
    virtual auto log(std::string_view log_message) const -> void = 0;
};

auto LoadModelFromMemory(std::span<const uint8_t> buffer, const GxxLogger *logger = nullptr) -> GxxModel;

auto CheckHeader(std::span<const uint8_t> buffer, const GxxLogger *logger = nullptr) -> bool;

} // namespace gxx
