#pragma once
#include <array>
#include <concepts>
#include <optional>
#include <string>
#include <variant>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "common/byteutils.hpp"

namespace act {

using namespace util::bytes;

struct ActVertexStatic {
    glm::fvec3 position;
    glm::fvec3 normal;
    glm::fvec2 texcoord;
    glm::fvec4 color;
    glm::fvec4 tangent;
};

struct ActVertexSkinned {
    glm::fvec3 position;
    glm::fvec3 normal;
    glm::fvec2 texcoord;
    glm::fvec4 color;
    glm::fvec4 tangent;

    std::array<uint32_t, 8> joints;
    std::array<float, 8> weights;
};

template <typename T>
concept VertexType = std::convertible_to<T, ActVertexStatic> || std::convertible_to<T, ActVertexSkinned>;

template <typename T>
concept VertexRange = std::ranges::range<T> && VertexType<std::ranges::range_value_t<T>>;

constexpr u32 kMagicNumber = 0x52544341;
constexpr u32 kWriterVersion = 0x20000000;
constexpr u32 kWriterSoftware = 0x20000000;

// clang-format off
enum class ActImageMimeType : u32 {
    eRawBitmap = 0x10000001,
    ePngBitmap = 0x10000002,
    eJpgBitmap = 0x10000003,
    eTgaBitmap = 0x10000004,
};
// clang-format on

// clang-format off
enum class ActTextureWrapType : u32 {
    eRepeat         = 0x10010001,
    eClampToEdge    = 0x10010002,
    eMirroredRepeat = 0x10010003,
};
// clang-format on

// clang-format off
enum class ActSubmeshVertexLayout : u32 {
    eVertexStatic       = 0x10020001,
    eVertexSkinned      = 0x10020002,
};
// clang-format on

// clang-format off
enum class ActInterpolationMode : u32 {
    eLinear         = 0x10030001,
    eCubicSpline    = 0x10030002,
    eStep           = 0x10030003,
    eSlerp          = 0x10030004,
    eHermite        = 0x10030005,
    eCubic          = 0x10030006,
};
// clang-format on

// clang-format off
enum class ActNodeProperty : u32 {
    eTranslation    = 0x10040001,
    eRotation       = 0x10040002,
    eScale          = 0x10040003,
    eUvOffset       = 0x10040004,
    eUvScale        = 0x10040005,
    eAlpha          = 0x10040006,
};
// clang-format on

// clang-format off
enum class ActBlockType : u32 {
    eImageBlock             = 0x20000001,
    eTextureBlock           = 0x20000002,
    eMaterialBlock          = 0x20000003,
    eNodeBlock              = 0x20000004,
    eMeshBlock              = 0x20000005,
    eSubmeshBlock           = 0x20000006,
    eSkinBlock              = 0x20000007,
    eSkinNodeBlock          = 0x20000008,
    eAnimationBlock         = 0x20000009,
    eAnimChannelBlock       = 0x2000000a,
};
// clang-format on

// clang-format off
enum class ActCommandType : u32 {
    eImageSetMimeType           = 0x30010001,   // [uint32]
    eImageSetDimensions         = 0x30010002,   // [uint32][uint32]
    eImageSetBuffer             = 0x30010003,   // [buffer]
    eImageSetName               = 0x30010004,   // [string]

    eTextureSetImage            = 0x30020001,   // [uint32]
    eTextureSetWrapS            = 0x30020002,   // [uint32]
    eTextureSetWrapT            = 0x30020003,   // [uint32]
    eTextureSetName             = 0x30020004,   // [string]

    eMaterialSetBaseColor       = 0x30030001,   // [float32][float32][float32][float32]
    eMaterialSetRoughness       = 0x30030002,   // [float32]
    eMaterialSetMetallic        = 0x30030003,   // [float32]
    eMaterialSetAlbedoMap       = 0x30030004,   // [uint32]
    eMaterialSetNormalMap       = 0x30030005,   // [uint32]
    eMaterialSetUvOffset        = 0x30030006,   // [uint32][uint32]
    eMaterialSetUvScale         = 0x30030007,   // [uint32][uint32]
    eMaterialSetAlpha           = 0x30030008,   // [uint32]
    eMaterialSetName            = 0x30030009,   // [string]

    eNodeSetTranslation         = 0x30040001,   // [float32][float32][float32]
    eNodeSetRotation            = 0x30040002,   // [float32][float32][float32][float32]
    eNodeSetScale               = 0x30040003,   // [float32][float32][float32]
    eNodeSetMesh                = 0x30040004,   // [uint32]
    eNodeSetSkin                = 0x30040005,   // [uint32]
    eNodeSetParent              = 0x30040006,   // [uint32]
    eNodeSetUvOffset            = 0x30040007,   // [float32][float32]
    eNodeSetUvScale             = 0x30040008,   // [float32][float32]
    eNodeSetAlpha               = 0x30040009,   // [float32]
    eNodeSetName                = 0x3004000a,   // [string]
    eNodeSetDrawOrder           = 0x3004000b,   // [uint32]
    eNodeSetBoundingBox         = 0x3004000c,   // [float32][6]

    eMeshAddSubmesh             = 0x30050001,   // [uint32]

    eSubmeshSetLayout           = 0x30060001,   // [uint32]
    eSubmeshSetVertices         = 0x30060002,   // [buffer]
    eSubmeshSetIndices          = 0x30060003,   // [buffer]
    eSubmeshSetMaterial         = 0x30060004,   // [uint32]
    eSubmeshSetSkin             = 0x30060005,   // [uint32]

    eSkinAddNode                = 0x30070001,   // [uint32]

    eSkinNodeSetNode            = 0x30080001,   // [uint32]
    eSkinNodeSetMatrix          = 0x30080002,   // [buffer]

    eAnimationAddChannel        = 0x30090001,   // [uint32]
    eAnimationSetName           = 0x30090002,   // [string]
    eAnimationAddFrameLoop      = 0x30090003,   // [float32][float32]

    eAnimChannelSetNode         = 0x300a0001,   // [uint32]
    eAnimChannelSetProp         = 0x300a0002,   // [uint32]
    eAnimChannelSetMode         = 0x300a0003,   // [uint32]
    eAnimChannelSetKeyframes    = 0x300a0004,   // [buffer] 
    eAnimChannelSetName         = 0x300a0005,   // [string] 
};
// clang-format on

template <ActNodeProperty> struct ActAnimPropertyTraits;

template <> struct ActAnimPropertyTraits<ActNodeProperty::eTranslation> {
    using DataType = glm::fvec3;
};

template <> struct ActAnimPropertyTraits<ActNodeProperty::eRotation> {
    using DataType = glm::fquat;
};

template <> struct ActAnimPropertyTraits<ActNodeProperty::eScale> {
    using DataType = glm::fvec3;
};

template <> struct ActAnimPropertyTraits<ActNodeProperty::eUvOffset> {
    using DataType = glm::fvec2;
};

template <> struct ActAnimPropertyTraits<ActNodeProperty::eUvScale> {
    using DataType = glm::fvec2;
};

template <> struct ActAnimPropertyTraits<ActNodeProperty::eAlpha> {
    using DataType = glm::fvec1;
};

struct ActImage final {
    std::string name;
    ActImageMimeType mime_type = ActImageMimeType::eRawBitmap;
    glm::uvec2 dimensions = glm::uvec2{0, 0};
    std::vector<u8> buffer = {};
};

struct ActTexture final {
    std::string name;
    u32 image_id = 0;

    ActTextureWrapType wrap_s = ActTextureWrapType::eRepeat;
    ActTextureWrapType wrap_t = ActTextureWrapType::eRepeat;
};

struct ActMaterial final {
    std::string name;

    std::optional<u32> albedo_map_id;
    std::optional<u32> normal_map_id;

    glm::fvec4 base_color = glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f};
    f32 roughness = 0.0f;
    f32 metallic = 0.0f;

    glm::fvec2 uv_offset = glm::fvec2{0.0f, 0.0f};
    glm::fvec2 uv_scale = glm::fvec2{1.0f, 1.0f};
    float alpha = 1.0f;
};

struct ActNode final {
    std::string name;

    glm::fvec3 translation = glm::fvec3{0.0f, 0.0f, 0.0f};
    glm::fvec3 scale = glm::fvec3{1.0f, 1.0f, 1.0f};
    glm::fquat rotation = glm::fquat{1.0f, 0.0f, 0.0f, 0.0f};
    glm::fvec2 uv_offset = glm::fvec2{0.0f, 0.0f};
    glm::fvec2 uv_scale = glm::fvec2{1.0f, 1.0f};

    float alpha = 1.0f;
    uint32_t draw_order = 0;

    glm::fvec3 bounding_min = glm::fvec3{0.0f, 0.0f, 0.0f};
    glm::fvec3 bounding_max = glm::fvec3{0.0f, 0.0f, 0.0f};

    std::optional<u32> mesh_id;
    std::optional<u32> skin_id;
    std::optional<u32> parent_id;
};

struct ActMesh final {
    std::string name;
    std::vector<u32> submesh_ids;
};

template <VertexType V> struct ActSubMesh final {
    ActSubmeshVertexLayout layout = ActSubmeshVertexLayout::eVertexStatic;
    std::vector<V> vertices;
    std::vector<u32> indices;
    u32 material = 0;
    std::optional<u32> skin_id;
};

struct ActSkin final {
    std::vector<u32> skin_node_ids;
};

struct ActSkinNode final {
    u32 node_id = 0;
    glm::fmat4x4 inverse_bind_matrix = glm::fmat4x4{1.0f};
};

struct ActAnimation final {
    std::string name;
    std::vector<u32> channel_ids;

    glm::fvec2 frame_loop = glm::fvec2{0.0f, 0.0f};
};

template <ActNodeProperty Property, ActInterpolationMode Mode> struct ActKeyframe;

template <ActNodeProperty Property> struct ActKeyframe<Property, ActInterpolationMode::eLinear> {};

template <ActNodeProperty Property> struct ActAnimationChannel final {
    static constexpr auto ActProperty = Property;
    using Traits = ActAnimPropertyTraits<Property>;

    u32 node_id;
    ActInterpolationMode interpolation;
    std::string name;

    struct ActKeyframe {
        float time = 0.0f;
        Traits::DataType value = {};
        Traits::DataType in_dy = {};
        Traits::DataType out_dy = {};
    };

    std::vector<ActKeyframe> keyframes;
};

using ActStaticSubmesh = ActSubMesh<ActVertexStatic>;
using ActSkinnedSubmesh = ActSubMesh<ActVertexSkinned>;
using ActAnySubmesh = std::variant<ActStaticSubmesh, ActSkinnedSubmesh>;

using ActRotationAnimationChannel = ActAnimationChannel<ActNodeProperty::eRotation>;
using ActScaleAnimationChannel = ActAnimationChannel<ActNodeProperty::eScale>;
using ActTranslationAnimationChannel = ActAnimationChannel<ActNodeProperty::eTranslation>;
using ActUvOffsetAnimationChannel = ActAnimationChannel<ActNodeProperty::eUvOffset>;
using ActUvScaleAnimationChannel = ActAnimationChannel<ActNodeProperty::eUvScale>;
using ActAlphaAnimationChannel = ActAnimationChannel<ActNodeProperty::eAlpha>;

using ActAnyAnimationChannel = std::variant<
    ActRotationAnimationChannel, ActScaleAnimationChannel, ActTranslationAnimationChannel, ActUvOffsetAnimationChannel,
    ActUvScaleAnimationChannel, ActAlphaAnimationChannel>;

struct ActModel final {
    std::vector<ActImage> images;
    std::vector<ActTexture> textures;
    std::vector<ActMaterial> materials;
    std::vector<ActNode> nodes;
    std::vector<ActMesh> meshes;
    std::vector<ActAnySubmesh> submeshes;
    std::vector<ActSkin> skins;
    std::vector<ActSkinNode> skin_nodes;
    std::vector<ActAnimation> animations;
    std::vector<ActAnyAnimationChannel> animation_channels;
};

auto LoadFromBinary(std::span<const u8> buffer) -> ActModel;
auto CheckHeader(std::span<const u8> binary) -> bool;

} // namespace act
