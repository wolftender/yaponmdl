#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <span>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace gmo {

using GmoFlags = uint32_t;

template <typename T>
    requires std::is_same_v<std::underlying_type_t<T>, GmoFlags>
auto operator|(const T &a, const T &b) -> T {
    return static_cast<T>(static_cast<std::underlying_type_t<T>>(a) | static_cast<std::underlying_type_t<T>>(b));
}

template <typename T>
    requires std::is_same_v<std::underlying_type_t<T>, GmoFlags>
auto operator&(const T &a, const T &b) -> T {
    return static_cast<T>(static_cast<std::underlying_type_t<T>>(a) & static_cast<std::underlying_type_t<T>>(b));
}

template <typename T>
    requires std::is_same_v<std::underlying_type_t<T>, GmoFlags>
auto operator~(const T &a) -> T {
    return static_cast<T>(~static_cast<std::underlying_type_t<T>>(a));
}

template <typename T> struct Rect {
    glm::vec<2, T> min;
    glm::vec<2, T> max;
};

/**
 * @brief This is a structure that describes the GMO vertex
 *
 * This is a structure that is more or less equivalent with what
 * the graphics unit of the PSP could do, notice that the setup is
 * a bit different to what is commonly used in formats such as
 * gltf these days, each vertex can specify up to 8 weights, those
 * weights refer to the blend bones, the blend bones are stored in
 * the bone structure (theres up to 8 of them)
 */
struct GmoVertex {
    glm::fvec3 position;
    glm::fvec3 normal;
    glm::fvec2 uv;
    glm::fvec4 color;

    std::array<float, 8> weights = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
};

// clang-format off
enum class GmoBoneFlags : GmoFlags {
    eBoneNone               = 0,
    eBoneHasTranslation     = 1 << 0,
    eBoneHasRotation        = 1 << 1,
    eBoneHasScale1          = 1 << 2,
    eBoneHasScale2          = 1 << 3,
    eBoneHasScale3          = 1 << 4,
    eBoneHasMultMatrix      = 1 << 5,
    eBoneHasPivot           = 1 << 6,
    eBoneHasMorphWeights    = 1 << 7,
    eBoneHasVisibility      = 1 << 8,
    eBoneHasBoundingBox     = 1 << 9,
    eBoneHasBlendBones      = 1 << 10,
};
// clang-format on

struct GmoBone {
    GmoBoneFlags flags = GmoBoneFlags::eBoneNone;

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

    glm::fvec3 bounding_min = {0.0f, 0.0f, 0.0f};
    glm::fvec3 bounding_max = {0.0f, 0.0f, 0.0f};

    glm::fmat4x4 local_matrix = {1.0f};
    glm::fmat4x4 stack_matrix = {1.0f};
    glm::fvec3 stack_scale = {0.0f, 0.0f, 0.0f};
    glm::fmat4x4 world_matrix = {1.0f};
};

// clang-format off
enum class GmoVertexArrayFlags : GmoFlags {
    eNone           = 0,
    eHasPositions   = 1 << 0,
    eHasNormals     = 1 << 1,
    eHasUvs         = 1 << 2,
    eHasWeights     = 1 << 3,
    eHasColor       = 1 << 4,
};
// clang-format on

struct GmoVertexArray {
    GmoVertexArrayFlags flags = GmoVertexArrayFlags::eNone;
    uint8_t num_weights = 0;

    std::vector<GmoVertex> vertices;
};

enum class GmoDrawPrimitive {
    ePoints,
    eLines,
    eLineStrip,
    eTriangles,
    eTriangleStrip,
    eTriangleFan,
    eRectangles,
};

struct GmoDrawArray {
    uint32_t array_id;             // reference to the array in the part arrays
    GmoDrawPrimitive primitive;    // primitive type to draw
    uint32_t num_vertices;         // how many vertices
    uint32_t num_primitives;       // how many primitives
    std::vector<uint32_t> indices; // index array (can be empty - unindexed draw)
};

struct GmoMesh {
    uint32_t material;
    std::vector<uint32_t> blend_subset;
    std::vector<GmoDrawArray> draw_arrays;
};

struct GmoPart {
    glm::fvec3 bounding_min = {0.0f, 0.0f, 0.0f};
    glm::fvec3 bounding_max = {0.0f, 0.0f, 0.0f};

    std::vector<GmoVertexArray> vertex_arrays;
    std::vector<GmoMesh> meshes;
};

struct GmoMaterial {};
struct GmoTexture {};
struct GmoMotion {};

// clang-format off
enum class GmoModelFlags : GmoFlags {
    eModelNone              = 0,
    eModelHasBoundingBox    = 1 << 0,
    eModelHasVertexOffset   = 1 << 1,
    eModelHasTextureOffset  = 1 << 2,
};
// clang-format on

struct GmoModel {
    GmoModelFlags flags = GmoModelFlags::eModelNone;

    std::vector<GmoBone> bones;
    std::vector<GmoPart> parts;
    std::vector<GmoMaterial> materials;
    std::vector<GmoTexture> textures;
    std::vector<GmoMotion> motions;

    glm::fmat4x4 world_matrix = {1.0f};
    glm::fvec3 bounding_min = {0.0f, 0.0f, 0.0f};
    glm::fvec3 bounding_max = {0.0f, 0.0f, 0.0f};
    glm::fmat4x4 vertex_offset = {1.0f};

    Rect<float> texture_offset = {{0.0f, 0.0f}, {0.0f, 0.0f}};
};

/**
 * @brief Load GMO models from the binary file
 *
 * @param buffer binary buffer to load from
 * @return std::vector<GmoModel> list of GMO models loaded from the file
 */
std::vector<GmoModel> LoadModelFromMemory(std::span<const uint8_t> buffer);

} // namespace gmo
