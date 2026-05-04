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

enum GxxPrimitiveType {
    eGxxPrimitivePoints,
    eGxxPrimitiveLines,
    eGxxPrimitiveLineStrip,
    eGxxPrimitiveTriangles,
    eGxxPrimitiveTriangleStrip,
    eGxxPrimitiveTriangleFan,
    eGxxPrimitiveCount,
};

struct GxxVertex {
    glm::fvec3 position;
    glm::fvec3 normal;
    glm::fvec3 color;
    glm::fvec2 uv;

    std::array<float, 8> weights;
};

struct GxxMesh {
    uint32_t id;

    std::vector<GxxVertex> vertices;
    std::vector<uint32_t> indices;

    GxxPrimitiveType primitive_type;
    uint32_t texture_id;
};

struct GxxTexture {
    uint32_t id;
    std::string name;
};

struct GxxBone {
    std::string name;
};

struct GxxDrawlist {
    std::optional<uint32_t> mesh = std::nullopt;
    std::optional<uint32_t> texture = std::nullopt;

    glm::fmat4x4 world_matrix = {1.0f};
    glm::fvec4 albedo_color = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::fvec2 uv_offset = {0.0f, 0.0f};
    glm::fvec2 uv_scale = {1.0f, 1.0f};
};

/**
 * @brief gxx frames are just GU drawlists
 * this tiny library exposes this fact though a very simple interface
 * where each frame is just a list of render parameters
 */
struct GxxAnimationFrame {
    std::vector<GxxDrawlist> list;
};

struct GxxAnimation {
    std::string name;
    uint32_t num_frames;
    float framerate;
    float frameloop;

    std::vector<GxxAnimationFrame> frames;
};

struct GxxModel {
    std::vector<GxxTexture> textures;
    std::vector<GxxBone> bones;
    std::vector<GxxMesh> meshes;
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
