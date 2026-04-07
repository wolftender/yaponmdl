#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <span>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace gmo {

enum class GmoResult {
    eFailure = 0,
    eSuccess = 1,
};

template <typename T> struct Rect {
    glm::vec<2, T> min;
    glm::vec<2, T> max;
};

struct GmoBone {
    uint32_t flags = 0;

    std::vector<uint32_t> draw_parts;
    std::vector<float> morph_weights;
    std::vector<int> blend_bones;
    std::vector<glm::fmat4x4> blend_offsets;

    std::optional<uint32_t> parent_bone;
    int32_t visibility = 0;
    int32_t anim_flags = 0;

    std::array<float, 5> anim_weights = {0, 0, 0, 0, 0};

    glm::fvec3 pivot = {0.0f, 0.0f, 0.0f};
    glm::fvec3 translation = {0.0f, 0.0f, 0.0f};
    glm::fquat rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    glm::fvec3 scale = {0.0f, 0.0f, 0.0f};

    glm::fmat4x4 local_matrix = {1.0f};
    glm::fmat4x4 stack_matrix = {1.0f};
    glm::fvec3 stack_scale = {0.0f, 0.0f, 0.0f};
    glm::fmat4x4 world_matrix = {1.0f};
};

struct GmoPart {};
struct GmoMaterial {};
struct GmoTexture {};
struct GmoMotion {};

struct GmoModel {
    uint32_t flags;

    std::vector<GmoBone> bones;
    std::vector<GmoPart> parts;
    std::vector<GmoMaterial> materials;
    std::vector<GmoTexture> textures;
    std::vector<GmoMotion> motions;

    glm::fmat4x4 world_matrix;
    glm::fvec3 bounding_min;
    glm::fvec3 bounding_max;
    glm::fmat4x4 vertex_offset;
    Rect<float> texture_offset;
};

/**
 * @brief Load GMO models from the binary file
 *
 * @param buffer binary buffer to load from
 * @return std::vector<GmoModel> list of GMO models loaded from the file
 */
std::vector<GmoModel> LoadModelFromMemory(std::span<const uint8_t> buffer);

} // namespace gmo
