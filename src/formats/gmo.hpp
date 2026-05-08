#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <span>
#include <string>

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

template <typename T>
    requires std::is_same_v<std::underlying_type_t<T>, GmoFlags>
auto FlagHas(const T &flagset, T flag) -> bool {
    return (static_cast<std::underlying_type_t<T>>(flagset) & static_cast<std::underlying_type_t<T>>(flag)) ==
           static_cast<std::underlying_type_t<T>>(flag);
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
    std::string name;

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
    glm::fvec3 scale = {1.0f, 1.0f, 1.0f};

    glm::fvec3 bounding_min = {0.0f, 0.0f, 0.0f};
    glm::fvec3 bounding_max = {0.0f, 0.0f, 0.0f};

    glm::fmat4x4 local_matrix = {1.0f};
    uint32_t draw_sort = 0;
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
    std::string name;

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
    std::string name;
    uint32_t material;
    std::vector<uint32_t> blend_subset;
    std::vector<GmoDrawArray> draw_arrays;
};

struct GmoPart {
    std::string name;

    glm::fvec3 bounding_min = {0.0f, 0.0f, 0.0f};
    glm::fvec3 bounding_max = {0.0f, 0.0f, 0.0f};

    std::vector<GmoVertexArray> vertex_arrays;
    std::vector<GmoMesh> meshes;
};

enum class GmoMaterialLayerFlags : GmoFlags {
    eNone = 0,
    eHasTextureCrop = 1 << 0,
};

enum class GmoMaterialLayerMapType {
    eNone,
    eDiffuse,
    eSpecular,
    eEmission,
    eAmbient,
    eReflection,
    eRefraction,
    eBump,
};

enum class GmoBlendOperator {
    eAdd,
    eSubtract,
    eReverseSubtract,
    eMin,
    eMax,
    eAbs,
};

enum class GmoBlendFunction {
    eSrcColor,
    eOneMinusSrcColor,
    eDstColor,
    eOneMinusDstColor,
    eSrcAlpha,
    eOneMinusSrcAlpha,
    eDstAlpha,
    eOneMinusDstAlpha,
    eDoubleSrcAlpha,
    eOneMinusDoubleSrcAlpha,
    eDoubleDstAlpha,
    eOneMinusDoubleDstAlpha,
    eFixValue,
};

struct GmoMaterialLayer {
    std::string name;

    GmoMaterialLayerFlags flags = GmoMaterialLayerFlags::eNone;
    GmoMaterialLayerMapType map_type = GmoMaterialLayerMapType::eNone;

    float map_factor = 0.0f;
    uint32_t texture_id;

    GmoBlendOperator blend_op = GmoBlendOperator::eAdd;
    GmoBlendFunction blend_func1 = GmoBlendFunction::eSrcAlpha;
    GmoBlendFunction blend_func2 = GmoBlendFunction::eOneMinusSrcAlpha;
    uint32_t src_mask = 0xffffffff;
    uint32_t dst_mask = 0xffffffff;

    Rect<float> texture_crop = {{0.0f, 0.0f}, {0.0f, 0.0f}};
};

// clang-format off
enum GmoMaterialColor {
    eGmoMaterialColorBlack      = 0,
    eGmoMaterialColorDiffuse    = 1,
    eGmoMaterialColorSpecular   = 2,
    eGmoMaterialColorEmission   = 3,
    eGmoMaterialColorAmbient    = 4,
    eGmoMaterialColorReflection = 5,
    eGmoMaterialColorCount
};
// clang-format on

// clang-format off
enum class GmoMaterialFlags : GmoFlags {
    eNone = 0,
    eEnableLighting     = 1 << 0,
    eEnableFog          = 1 << 1,
    eEnableCullFace     = 1 << 2,
    eEnableDepthTest    = 1 << 3,
    eEnableDepthMask    = 1 << 4,
    eEnableAlphaTest    = 1 << 5,
    eEnableAlphaMask    = 1 << 6,
    eHasDiffuse         = 1 << 7,
    eHasSpecular        = 1 << 8,
    eHasEmission        = 1 << 9,
    eHasAmbient         = 1 << 10,
    eHasReflection      = 1 << 11,
    eHasRefraction      = 1 << 12,
};
// clang-format on

struct GmoMaterial {
    GmoMaterialFlags flags = GmoMaterialFlags::eNone;
    std::string name;

    std::vector<GmoMaterialLayer> layers;

    // clang-format off
    std::array<glm::fvec4, eGmoMaterialColorCount> colors = {
        glm::fvec4{0.0f, 0.0f, 0.0f, 1.0f},
        glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f},
        glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f},
        glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f},
        glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f},
        glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f},
    };
    // clang-format on

    float shininess = 0.0f;
    float refraction = 1.0f;
    float bump = 0.0f;
};

struct GmoTexture {
    std::string name;
    std::string filename;
    std::vector<uint8_t> data;
};

enum class GmoAnimationTarget {
    eBone,
    eMaterial,
    eUnknown,
};

enum GmoAnimationProperty {
    eAnimBoneTranslate,
    eAnimBoneRotateQ,
    eAnimBoneRotateZYX,
    eAnimBoneRotateYXZ,
    eAnimBoneScale1,
    eAnimBoneScale2,
    eAnimBoneScale3,
    eAnimBoneMultMatrix,
    eAnimBoneMorphWeights,
    eAnimBoneMorphIndex,
    eAnimBoneVisibility,
    eAnimMaterialDiffuse,
    eAnimMaterialSpecular,
    eAnimMaterialEmission,
    eAnimMaterialAmbient,
    eAnimMaterialReflection,
    eAnimMaterialRefraction,
    eAnimMaterialBump,
    eAnimMaterialTextureCrop,
    eAnimPataponTextureEXT = 0x4100,
    eAnimPataponUnknownEXT = 0x4101,
};

struct GmoAnimation {
    std::string name;
    uint32_t fcurve_id;
    GmoAnimationTarget target;
    uint32_t target_id;
    GmoAnimationProperty property;
    uint32_t index;
};

enum class GmoFCurveInterpolation : uint32_t {
    eConstant = 0,
    eLinear = 1,
    eHermite = 2,
    eCubic = 3,
    eSpherical = 4,
};

struct GmoFCurve {
    std::string name;
    uint32_t dimensions;
    uint32_t num_keyframes;

    GmoFCurveInterpolation interpolation;

    /// data is stored in the same format as in GMO fcurve
    /// that is
    /// [time] [value]
    /// you can calculate the stride with this formula:
    ///  num_elements = {1, 1, 3, 5, 1}[interpolation];
    ///  stride = num_elements * dimensions + 1;
    ///
    std::vector<float> raw_data;
};

constexpr auto NumElementsPerInterpType(GmoFCurveInterpolation interpolation) -> uint32_t {
    constexpr std::array<uint32_t, 5> kElementsPerInterpType = {1, 1, 3, 5, 1};
    const auto index = static_cast<uint32_t>(interpolation);

    if (index < kElementsPerInterpType.size()) {
        return kElementsPerInterpType[index];
    }

    return 0;
}

struct GmoMotion {
    std::string name;

    std::vector<GmoAnimation> animations;
    std::vector<GmoFCurve> fcurves;

    float frame_loop_start;
    float frame_loop_end;

    float framerate;
};

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
    std::string name;

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

class GmoLogger {
public:
    virtual ~GmoLogger() = default;
    virtual auto log(std::string_view log_message) const -> void = 0;
};

/**
 * @brief Load GMO models from the binary file
 *
 * @param buffer binary buffer to load from
 * @return std::vector<GmoModel> list of GMO models loaded from the file
 */
auto LoadModelFromMemory(std::span<const uint8_t> buffer, const GmoLogger *logger = nullptr) -> std::vector<GmoModel>;

auto CheckHeader(std::span<const uint8_t> buffer, const GmoLogger *logger = nullptr) -> bool;

} // namespace gmo
