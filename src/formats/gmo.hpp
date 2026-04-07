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

struct GmoBone {
    std::vector<uint32_t> draw_parts;
    std::vector<float> morph_weights;
    std::vector<int> blend_bones;
    std::vector<glm::fmat4x4> blend_offsets;

    std::optional<uint32_t> parent_bone;
    int32_t visibility;
    int32_t anim_flags;

    std::array<float, 5> anim_weights;

    glm::fvec3 pivot;
    glm::fvec3 translation;
    glm::fquat rotation;
    glm::fvec3 scale;

    glm::fmat4x4 local_matrix;
    glm::fmat4x4 stack_matrix;
    glm::fvec3 stack_scale;
    glm::fmat4x4 world_matrix;
};

struct GmoPart {};

struct GmoModel {};

/**
 * @brief Load GMO models from the binary file
 *
 * @param buffer binary buffer to load from
 * @return std::vector<GmoModel> list of GMO models loaded from the file
 */
std::vector<GmoModel> LoadModelFromMemory(std::span<const uint8_t> buffer);

} // namespace gmo
