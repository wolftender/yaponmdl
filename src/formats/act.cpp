#include <stdexcept>

#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>

#include "formats/act.hpp"
#include "common/binaryreader.hpp"

namespace act {

constexpr auto kActByteOrder = BinaryReader::ByteOrder::eLittleEndian;

constexpr auto ValueToActImageMimeType(u32 value) -> std::optional<ActImageMimeType> {
    switch (static_cast<ActImageMimeType>(value)) {
    case ActImageMimeType::eRawBitmap:
    case ActImageMimeType::ePngBitmap:
    case ActImageMimeType::eJpgBitmap:
    case ActImageMimeType::eTgaBitmap:
        return static_cast<ActImageMimeType>(value);
    default:
        return std::nullopt;
    }
}

constexpr auto ValueToActTextureWrapType(u32 value) -> std::optional<ActTextureWrapType> {
    switch (static_cast<ActTextureWrapType>(value)) {
    case ActTextureWrapType::eRepeat:
    case ActTextureWrapType::eClampToEdge:
    case ActTextureWrapType::eMirroredRepeat:
        return static_cast<ActTextureWrapType>(value);
    default:
        return std::nullopt;
    }
}

constexpr auto ValueToActSubmeshVertexLayout(u32 value) -> std::optional<ActSubmeshVertexLayout> {
    switch (static_cast<ActSubmeshVertexLayout>(value)) {
    case ActSubmeshVertexLayout::eVertexStatic:
    case ActSubmeshVertexLayout::eVertexSkinned:
        return static_cast<ActSubmeshVertexLayout>(value);
    default:
        return std::nullopt;
    }
}

constexpr auto ValueToActInterpolationMode(u32 value) -> std::optional<ActInterpolationMode> {
    switch (static_cast<ActInterpolationMode>(value)) {
    case ActInterpolationMode::eLinear:
    case ActInterpolationMode::eCubicSpline:
    case ActInterpolationMode::eStep:
    case ActInterpolationMode::eSlerp:
    case ActInterpolationMode::eHermite:
    case ActInterpolationMode::eCubic:
        return static_cast<ActInterpolationMode>(value);
    default:
        return std::nullopt;
    }
}

constexpr auto ValueToActNodeProperty(u32 value) -> std::optional<ActNodeProperty> {
    switch (static_cast<ActNodeProperty>(value)) {
    case ActNodeProperty::eTranslation:
    case ActNodeProperty::eRotation:
    case ActNodeProperty::eScale:
    case ActNodeProperty::eUvOffset:
    case ActNodeProperty::eUvScale:
    case ActNodeProperty::eAlpha:
        return static_cast<ActNodeProperty>(value);
    default:
        return std::nullopt;
    }
}

constexpr auto ValueToActBlockType(u32 value) -> std::optional<ActBlockType> {
    switch (static_cast<ActBlockType>(value)) {
    case ActBlockType::eImageBlock:
    case ActBlockType::eTextureBlock:
    case ActBlockType::eMaterialBlock:
    case ActBlockType::eNodeBlock:
    case ActBlockType::eMeshBlock:
    case ActBlockType::eSubmeshBlock:
    case ActBlockType::eSkinBlock:
    case ActBlockType::eSkinNodeBlock:
    case ActBlockType::eAnimationBlock:
    case ActBlockType::eAnimChannelBlock:
        return static_cast<ActBlockType>(value);
    default:
        return std::nullopt;
    }
}

constexpr auto ValueToActCommandType(u32 value) -> std::optional<ActCommandType> {
    switch (static_cast<ActCommandType>(value)) {
    case ActCommandType::eImageSetMimeType:
    case ActCommandType::eImageSetDimensions:
    case ActCommandType::eImageSetBuffer:
    case ActCommandType::eImageSetName:
    case ActCommandType::eTextureSetImage:
    case ActCommandType::eTextureSetWrapS:
    case ActCommandType::eTextureSetWrapT:
    case ActCommandType::eTextureSetName:
    case ActCommandType::eMaterialSetBaseColor:
    case ActCommandType::eMaterialSetRoughness:
    case ActCommandType::eMaterialSetMetallic:
    case ActCommandType::eMaterialSetAlbedoMap:
    case ActCommandType::eMaterialSetNormalMap:
    case ActCommandType::eMaterialSetUvOffset:
    case ActCommandType::eMaterialSetUvScale:
    case ActCommandType::eMaterialSetAlpha:
    case ActCommandType::eMaterialSetName:
    case ActCommandType::eNodeSetTranslation:
    case ActCommandType::eNodeSetRotation:
    case ActCommandType::eNodeSetScale:
    case ActCommandType::eNodeSetMesh:
    case ActCommandType::eNodeSetSkin:
    case ActCommandType::eNodeSetParent:
    case ActCommandType::eNodeSetUvOffset:
    case ActCommandType::eNodeSetUvScale:
    case ActCommandType::eNodeSetAlpha:
    case ActCommandType::eNodeSetName:
    case ActCommandType::eNodeSetDrawOrder:
    case ActCommandType::eNodeSetBoundingBox:
    case ActCommandType::eMeshAddSubmesh:
    case ActCommandType::eSubmeshSetLayout:
    case ActCommandType::eSubmeshSetVertices:
    case ActCommandType::eSubmeshSetSkin:
    case ActCommandType::eSubmeshSetIndices:
    case ActCommandType::eSubmeshSetMaterial:
    case ActCommandType::eSkinAddNode:
    case ActCommandType::eSkinNodeSetNode:
    case ActCommandType::eSkinNodeSetMatrix:
    case ActCommandType::eAnimationAddChannel:
    case ActCommandType::eAnimationSetName:
    case ActCommandType::eAnimationAddFrameLoop:
    case ActCommandType::eAnimChannelSetNode:
    case ActCommandType::eAnimChannelSetProp:
    case ActCommandType::eAnimChannelSetMode:
    case ActCommandType::eAnimChannelSetKeyframes:
    case ActCommandType::eAnimChannelSetName:
        return static_cast<ActCommandType>(value);
    default:
        return std::nullopt;
    }
}

template <typename T>
inline auto AssertRead(util::bytes::BinaryReader &reader, std::optional<std::string_view> message = std::nullopt) -> T {
    const auto value = reader.Read<kActByteOrder, T>();
    if (!value.has_value()) {
        if (message.has_value()) {
            throw std::runtime_error{std::string{message.value()}};
        } else {
            throw std::runtime_error{fmt::format("invalid byte read at position {}", reader.Position())};
        }
    }

    return value.value();
}

template <typename T, size_t N> auto AssertReadVector(util::bytes::BinaryReader &reader) -> glm::vec<N, T> {
    glm::vec<N, T> result;
    for (uint32_t i = 0; i < N; ++i) {
        result[i] = AssertRead<T>(reader);
    }

    return result;
}

inline auto ReadCommandType(util::bytes::BinaryReader &reader) -> std::optional<ActCommandType> {
    const auto raw_value = reader.Read<kActByteOrder, u32>();

    if (!raw_value.has_value()) {
        return std::nullopt;
    }

    return ValueToActCommandType(raw_value.value());
}

template <IsPrimitiveType T> inline auto ReadCommandParameter(util::bytes::BinaryReader &reader) -> T {
    const auto size_value = reader.Read<kActByteOrder, u32>();
    if (!size_value.has_value()) {
        throw std::runtime_error{"invalid command, size value is not present"};
    }

    const auto data_size = size_value.value();
    if (data_size != sizeof(T)) {
        throw std::runtime_error{fmt::format("command size mismatch: {} != {}", data_size, sizeof(T))};
    }

    const auto parameter = reader.Read<kActByteOrder, T>();
    if (!parameter.has_value()) {
        throw std::runtime_error{fmt::format("incomplete command, expected {} bytes for value", data_size)};
    }

    return parameter.value();
}

template <typename T, std::invocable<u32> F>
    requires std::is_same_v<std::underlying_type_t<T>, u32> &&
             std::is_same_v<std::invoke_result_t<F, u32>, std::optional<T>>
inline auto ReadCommandParameterEnum(util::bytes::BinaryReader &reader, F fnParser) -> T {
    const auto raw = ReadCommandParameter<u32>(reader);
    const auto parsed = fnParser(raw);

    if (!parsed.has_value()) {
        throw std::runtime_error{fmt::format("invalid value for enumeration {:#010x}", raw)};
    }

    return parsed.value();
}

inline auto ReadCommandParameterBuffer(util::bytes::BinaryReader &reader) -> std::span<const u8> {
    const auto size_value = reader.Read<kActByteOrder, u32>();
    if (!size_value.has_value()) {
        throw std::runtime_error{"invalid command, size value is not present"};
    }

    const auto data_size = size_value.value();
    const auto view = reader.ReadBuffer(data_size);

    if (!view.has_value()) {
        throw std::runtime_error{fmt::format("incomplete command, expected {} bytes for buffer", data_size)};
    }

    return view.value();
}

inline auto ReadCommandParameterString(util::bytes::BinaryReader &reader) -> std::string_view {
    const auto buffer_view = ReadCommandParameterBuffer(reader);
    return std::string_view{std::bit_cast<const char *>(buffer_view.data()), buffer_view.size_bytes()};
}

template <typename T, int N> auto ReadCommandParameterVec(util::bytes::BinaryReader &reader) -> glm::vec<N, T> {
    const auto size_value = reader.Read<kActByteOrder, u32>();
    if (!size_value.has_value()) {
        throw std::runtime_error{"invalid command, size value is not present"};
    }

    const auto data_size = size_value.value();
    if (data_size != sizeof(T) * N) {
        throw std::runtime_error{fmt::format("command size mismatch: {} != {}", data_size, N * sizeof(T))};
    }

    glm::vec<N, T> value;
    for (int i = 0; i < N; ++i) {
        const auto opt = reader.Read<kActByteOrder, T>();
        if (!opt.has_value()) {
            throw std::runtime_error{fmt::format("not enough data for vector of size {}", N)};
        }

        value[i] = opt.value();
    }

    return value;
}

auto ReadCommandParameterQuat(util::bytes::BinaryReader &reader) -> glm::fquat {
    const auto size_value = reader.Read<kActByteOrder, u32>();
    if (!size_value.has_value()) {
        throw std::runtime_error{"invalid command, size value is not present"};
    }

    const auto data_size = size_value.value();
    if (data_size != sizeof(f32) * 4) {
        throw std::runtime_error{fmt::format("command size mismatch: {} != {}", data_size, 4 * sizeof(f32))};
    }

    const auto value = reader.ReadQuat<kActByteOrder>();
    if (!value.has_value()) {
        throw std::runtime_error{fmt::format("cannot parse quaternion")};
    }

    return value.value();
}

auto ReadImageBlock(std::span<const u8> buffer) -> ActImage {
    util::bytes::BinaryReader reader{buffer};
    ActImage image;

    while (const auto command = ReadCommandType(reader)) {
        switch (*command) {
        case ActCommandType::eImageSetMimeType:
            image.mime_type = ReadCommandParameterEnum<ActImageMimeType>(reader, ValueToActImageMimeType);
            break;

        case ActCommandType::eImageSetDimensions:
            image.dimensions = ReadCommandParameterVec<u32, 2>(reader);
            break;

        case ActCommandType::eImageSetBuffer: {
            const auto view = ReadCommandParameterBuffer(reader);
            image.buffer.assign(view.begin(), view.end());
            break;
        }

        case ActCommandType::eImageSetName:
            image.name = ReadCommandParameterString(reader);
            break;

        default:
            throw std::runtime_error{fmt::format("invalid image command {:#010x}", static_cast<u32>(*command))};
        }
    }

    return image;
}

auto ReadTextureBlock(std::span<const u8> buffer) -> ActTexture {
    util::bytes::BinaryReader reader{buffer};
    ActTexture texture;

    while (const auto command = ReadCommandType(reader)) {
        switch (*command) {
        case ActCommandType::eTextureSetImage:
            texture.image_id = ReadCommandParameter<u32>(reader);
            break;

        case ActCommandType::eTextureSetWrapS:
            texture.wrap_s = ReadCommandParameterEnum<ActTextureWrapType>(reader, ValueToActTextureWrapType);
            break;

        case ActCommandType::eTextureSetWrapT:
            texture.wrap_t = ReadCommandParameterEnum<ActTextureWrapType>(reader, ValueToActTextureWrapType);
            break;

        case ActCommandType::eTextureSetName:
            texture.name = ReadCommandParameterString(reader);
            break;

        default:
            throw std::runtime_error{fmt::format("invalid texture command {:#010x}", static_cast<u32>(*command))};
        }
    }

    return texture;
}

auto ReadMaterialBlock(std::span<const u8> buffer) -> ActMaterial {
    util::bytes::BinaryReader reader{buffer};
    ActMaterial material;

    while (const auto command = ReadCommandType(reader)) {
        switch (*command) {
        case ActCommandType::eMaterialSetBaseColor:
            material.base_color = ReadCommandParameterVec<f32, 4>(reader);
            break;

        case ActCommandType::eMaterialSetRoughness:
            material.roughness = ReadCommandParameter<f32>(reader);
            break;

        case ActCommandType::eMaterialSetMetallic:
            material.metallic = ReadCommandParameter<f32>(reader);
            break;

        case ActCommandType::eMaterialSetAlbedoMap:
            material.albedo_map_id = ReadCommandParameter<u32>(reader);
            break;

        case ActCommandType::eMaterialSetNormalMap:
            material.normal_map_id = ReadCommandParameter<u32>(reader);
            break;

        case ActCommandType::eMaterialSetUvOffset:
            material.uv_offset = ReadCommandParameterVec<f32, 2>(reader);
            break;

        case ActCommandType::eMaterialSetUvScale:
            material.uv_scale = ReadCommandParameterVec<f32, 2>(reader);
            break;

        case ActCommandType::eMaterialSetAlpha:
            material.alpha = ReadCommandParameter<float>(reader);
            break;

        case ActCommandType::eMaterialSetName:
            material.name = ReadCommandParameterString(reader);
            break;

        default:
            throw std::runtime_error{fmt::format("invalid material command {:#010x}", static_cast<u32>(*command))};
        }
    }

    return material;
}

auto ReadNodeBlock(std::span<const u8> buffer) -> ActNode {
    util::bytes::BinaryReader reader{buffer};
    ActNode node;

    while (const auto command = ReadCommandType(reader)) {
        switch (*command) {
        case ActCommandType::eNodeSetTranslation:
            node.translation = ReadCommandParameterVec<f32, 3>(reader);
            break;

        case ActCommandType::eNodeSetRotation:
            node.rotation = ReadCommandParameterQuat(reader);
            break;

        case ActCommandType::eNodeSetScale:
            node.scale = ReadCommandParameterVec<f32, 3>(reader);
            break;

        case ActCommandType::eNodeSetMesh:
            node.mesh_id = ReadCommandParameter<u32>(reader);
            break;

        case ActCommandType::eNodeSetSkin:
            node.skin_id = ReadCommandParameter<u32>(reader);
            break;

        case ActCommandType::eNodeSetParent:
            node.parent_id = ReadCommandParameter<u32>(reader);
            break;

        case ActCommandType::eNodeSetUvOffset:
            node.uv_offset = ReadCommandParameterVec<f32, 2>(reader);
            break;

        case ActCommandType::eNodeSetUvScale:
            node.uv_scale = ReadCommandParameterVec<f32, 2>(reader);
            break;

        case ActCommandType::eNodeSetAlpha:
            node.alpha = ReadCommandParameter<f32>(reader);
            break;

        case ActCommandType::eNodeSetName:
            node.name = ReadCommandParameterString(reader);
            break;

        case ActCommandType::eNodeSetDrawOrder:
            node.draw_order = ReadCommandParameter<u32>(reader);
            break;

        case ActCommandType::eNodeSetBoundingBox: {
            const auto size_value = reader.Read<kActByteOrder, u32>();
            if (!size_value.has_value()) {
                throw std::runtime_error{"invalid command, size value is not present"};
            }

            const auto data_size = size_value.value();
            if (data_size != sizeof(f32) * 6) {
                throw std::runtime_error{fmt::format("command size mismatch: {} != {}", data_size, sizeof(f32) * 6)};
            }

            for (int i = 0; i < 3; ++i) {
                const auto opt = reader.Read<kActByteOrder, f32>();
                if (!opt.has_value()) {
                    throw std::runtime_error{fmt::format("not enough data for vector of size 3")};
                }

                node.bounding_min[i] = opt.value();
            }

            for (int i = 0; i < 3; ++i) {
                const auto opt = reader.Read<kActByteOrder, f32>();
                if (!opt.has_value()) {
                    throw std::runtime_error{fmt::format("not enough data for vector of size 3")};
                }

                node.bounding_max[i] = opt.value();
            }

            break;
        }

        default:
            throw std::runtime_error{fmt::format("invalid node command {:#010x}", static_cast<u32>(*command))};
        }
    }

    return node;
}

auto ReadMeshBlock(std::span<const u8> buffer) -> ActMesh {
    util::bytes::BinaryReader reader{buffer};
    ActMesh mesh;

    while (const auto command = ReadCommandType(reader)) {
        switch (*command) {
        case ActCommandType::eMeshAddSubmesh:
            mesh.submesh_ids.emplace_back(ReadCommandParameter<u32>(reader));
            break;

        default:
            throw std::runtime_error{fmt::format("invalid mesh command {:#010x}", static_cast<u32>(*command))};
        }
    }

    return mesh;
}

template <IsPrimitiveType T> auto ParseBuffer(std::span<const u8> buffer) -> std::vector<T> {
    util::bytes::BinaryReader reader{buffer};
    const auto num_elements = buffer.size_bytes() / sizeof(T);

    std::vector<T> values;
    values.resize(num_elements);

    for (uint32_t i = 0; i < num_elements; ++i) {
        values[i] = AssertRead<T>(reader);
    }

    return values;
}

auto ParseMatrixBuffer(std::span<const u8> buffer) -> glm::fmat4x4 {
    if (buffer.size_bytes() != sizeof(f32) * 16) {
        throw std::runtime_error{
            fmt::format("invalid matrix4x4 buffer with size {} (expected: {})", buffer.size_bytes(), sizeof(f32) * 16)};
    }

    const auto float_values = ParseBuffer<f32>(buffer);
    return glm::make_mat4x4(float_values.data());
}

template <VertexType V> auto ParseVertexBuffer(std::span<const u8> buffer) -> std::vector<V>;

template <> auto ParseVertexBuffer<ActVertexStatic>(std::span<const u8> buffer) -> std::vector<ActVertexStatic> {
    constexpr size_t kBytesPerVertex = sizeof(f32) * 16;
    util::bytes::BinaryReader reader{buffer};

    const auto num_vertices = buffer.size_bytes() / kBytesPerVertex;
    std::vector<ActVertexStatic> vertices;

    vertices.resize(num_vertices);

    for (uint32_t i = 0; i < num_vertices; ++i) {
        auto &vertex = vertices[i];
        vertex.position = AssertReadVector<f32, 3>(reader);
        vertex.normal = AssertReadVector<f32, 3>(reader);
        vertex.texcoord = AssertReadVector<f32, 2>(reader);
        vertex.color = AssertReadVector<f32, 4>(reader);
        vertex.tangent = AssertReadVector<f32, 4>(reader);
    }

    return vertices;
}

template <> auto ParseVertexBuffer<ActVertexSkinned>(std::span<const u8> buffer) -> std::vector<ActVertexSkinned> {
    constexpr size_t kBytesPerVertex = sizeof(f32) * 24 + sizeof(u32) * 8;
    util::bytes::BinaryReader reader{buffer};

    const auto num_vertices = buffer.size_bytes() / kBytesPerVertex;
    std::vector<ActVertexSkinned> vertices;

    vertices.resize(num_vertices);

    for (uint32_t i = 0; i < num_vertices; ++i) {
        auto &vertex = vertices[i];
        vertex.position = AssertReadVector<f32, 3>(reader);
        vertex.normal = AssertReadVector<f32, 3>(reader);
        vertex.texcoord = AssertReadVector<f32, 2>(reader);
        vertex.color = AssertReadVector<f32, 4>(reader);
        vertex.tangent = AssertReadVector<f32, 4>(reader);

        vertex.joints[0] = AssertRead<u32>(reader);
        vertex.joints[1] = AssertRead<u32>(reader);
        vertex.joints[2] = AssertRead<u32>(reader);
        vertex.joints[3] = AssertRead<u32>(reader);
        vertex.joints[4] = AssertRead<u32>(reader);
        vertex.joints[5] = AssertRead<u32>(reader);
        vertex.joints[6] = AssertRead<u32>(reader);
        vertex.joints[7] = AssertRead<u32>(reader);

        vertex.weights[0] = AssertRead<f32>(reader);
        vertex.weights[1] = AssertRead<f32>(reader);
        vertex.weights[2] = AssertRead<f32>(reader);
        vertex.weights[3] = AssertRead<f32>(reader);
        vertex.weights[4] = AssertRead<f32>(reader);
        vertex.weights[5] = AssertRead<f32>(reader);
        vertex.weights[6] = AssertRead<f32>(reader);
        vertex.weights[7] = AssertRead<f32>(reader);
    }

    return vertices;
}

auto ReadSubmeshBlock(std::span<const u8> buffer) -> ActAnySubmesh {
    util::bytes::BinaryReader reader{buffer};

    std::optional<ActSubmeshVertexLayout> layout = std::nullopt;
    std::optional<std::span<const u8>> index_buffer = std::nullopt;
    std::optional<std::span<const u8>> vertex_buffer = std::nullopt;
    std::optional<u32> material_id = std::nullopt;
    std::optional<u32> skin_id = std::nullopt;

    while (const auto command = ReadCommandType(reader)) {
        switch (*command) {
        case ActCommandType::eSubmeshSetLayout:
            layout = ReadCommandParameterEnum<ActSubmeshVertexLayout>(reader, ValueToActSubmeshVertexLayout);
            break;

        case ActCommandType::eSubmeshSetMaterial:
            material_id = ReadCommandParameter<u32>(reader);
            break;

        case ActCommandType::eSubmeshSetSkin:
            skin_id = ReadCommandParameter<u32>(reader);
            break;

        case ActCommandType::eSubmeshSetVertices:
            vertex_buffer = ReadCommandParameterBuffer(reader);
            break;

        case ActCommandType::eSubmeshSetIndices:
            index_buffer = ReadCommandParameterBuffer(reader);
            break;

        default:
            throw std::runtime_error{fmt::format("invalid submesh command {:#010x}", static_cast<u32>(*command))};
        }
    }

    // build submesh
    if (!layout.has_value() || !index_buffer.has_value() || !vertex_buffer.has_value() || !material_id.has_value()) {
        throw std::runtime_error{"incomplete submesh block, cannot parse"};
    }

    switch (layout.value()) {
    case ActSubmeshVertexLayout::eVertexStatic:
        return ActStaticSubmesh{
            .layout = layout.value(),
            .vertices = ParseVertexBuffer<ActVertexStatic>(vertex_buffer.value()),
            .indices = ParseBuffer<u32>(index_buffer.value()),
            .material = material_id.value(),
        };
    case ActSubmeshVertexLayout::eVertexSkinned:
        return ActSkinnedSubmesh{
            .layout = layout.value(),
            .vertices = ParseVertexBuffer<ActVertexSkinned>(vertex_buffer.value()),
            .indices = ParseBuffer<u32>(index_buffer.value()),
            .material = material_id.value(),
            .skin_id = skin_id,
        };
    default:
        throw std::runtime_error{"invalid vertex layout for submesh"};
    }
}

auto ReadSkinBlock(std::span<const u8> buffer) -> ActSkin {
    util::bytes::BinaryReader reader{buffer};
    ActSkin skin;

    while (const auto command = ReadCommandType(reader)) {
        switch (*command) {
        case ActCommandType::eSkinAddNode:
            skin.skin_node_ids.emplace_back(ReadCommandParameter<u32>(reader));
            break;

        default:
            throw std::runtime_error{fmt::format("invalid skin command {:#010x}", static_cast<u32>(*command))};
        }
    }

    return skin;
}

auto ReadSkinNodeBlock(std::span<const u8> buffer) -> ActSkinNode {
    util::bytes::BinaryReader reader{buffer};
    ActSkinNode skin_node;

    while (const auto command = ReadCommandType(reader)) {
        switch (*command) {
        case ActCommandType::eSkinNodeSetNode:
            skin_node.node_id = ReadCommandParameter<u32>(reader);
            break;

        case ActCommandType::eSkinNodeSetMatrix:
            skin_node.inverse_bind_matrix = ParseMatrixBuffer(ReadCommandParameterBuffer(reader));
            break;

        default:
            throw std::runtime_error{fmt::format("invalid skin node command {:#010x}", static_cast<u32>(*command))};
        }
    }

    return skin_node;
}

auto ReadAnimationBlock(std::span<const u8> buffer) -> ActAnimation {
    util::bytes::BinaryReader reader{buffer};
    ActAnimation animation;

    while (const auto command = ReadCommandType(reader)) {
        switch (*command) {
        case ActCommandType::eAnimationAddChannel:
            animation.channel_ids.emplace_back(ReadCommandParameter<u32>(reader));
            break;

        case ActCommandType::eAnimationAddFrameLoop:
            animation.frame_loop = ReadCommandParameterVec<f32, 2>(reader);
            break;

        case ActCommandType::eAnimationSetName:
            animation.name = ReadCommandParameterString(reader);
            break;

        default:
            throw std::runtime_error{fmt::format("invalid animation command {:#010x}", static_cast<u32>(*command))};
        }
    }

    return animation;
}

template <typename T> struct GlmNumComponents;

template <glm::length_t L, typename T>
struct GlmNumComponents<glm::vec<L, T>> : std::integral_constant<glm::length_t, L> {};

template <ActNodeProperty Property>
auto ReadKeyframes(std::span<const u8> buffer, ActAnimationChannel<Property> &channel) -> void {
    constexpr auto dimensions = GlmNumComponents<typename ActAnimationChannel<Property>::Traits::DataType>::value;
    const auto num_values_per_keyframe = 1 + dimensions;
    const auto num_keyframes = buffer.size_bytes() / sizeof(f32) / num_values_per_keyframe;

    util::bytes::BinaryReader reader{buffer};
    channel.keyframes.resize(num_keyframes);

    for (uint32_t i = 0; i < num_keyframes; ++i) {
        auto &keyframe = channel.keyframes[i];

        keyframe.time = AssertRead<f32>(reader);
        keyframe.value = AssertReadVector<f32, dimensions>(reader);
    }
}

template <>
auto ReadKeyframes<ActNodeProperty::eRotation>(
    std::span<const u8> buffer, ActAnimationChannel<ActNodeProperty::eRotation> &channel) -> void {
    const auto num_values_per_keyframe = 5;
    const auto num_keyframes = buffer.size_bytes() / sizeof(f32) / num_values_per_keyframe;

    util::bytes::BinaryReader reader{buffer};
    channel.keyframes.resize(num_keyframes);

    for (uint32_t i = 0; i < num_keyframes; ++i) {
        auto &keyframe = channel.keyframes[i];

        keyframe.time = AssertRead<f32>(reader);
        keyframe.value.x = AssertRead<f32>(reader);
        keyframe.value.y = AssertRead<f32>(reader);
        keyframe.value.z = AssertRead<f32>(reader);
        keyframe.value.w = AssertRead<f32>(reader);
    }
}

template <ActNodeProperty Property>
auto ReadKeyframesHermite(std::span<const u8> buffer, ActAnimationChannel<Property> &channel) -> void {
    constexpr auto dimensions = GlmNumComponents<typename ActAnimationChannel<Property>::Traits::DataType>::value;
    const auto num_values_per_keyframe = 1 + 3 * dimensions;
    const auto num_keyframes = buffer.size_bytes() / sizeof(f32) / num_values_per_keyframe;

    util::bytes::BinaryReader reader{buffer};
    channel.keyframes.resize(num_keyframes);

    for (uint32_t i = 0; i < num_keyframes; ++i) {
        auto &keyframe = channel.keyframes[i];

        keyframe.time = AssertRead<f32>(reader);
        keyframe.value = AssertReadVector<f32, dimensions>(reader);
        keyframe.in_dy = AssertReadVector<f32, dimensions>(reader);
        keyframe.out_dy = AssertReadVector<f32, dimensions>(reader);
    }
}

template <>
auto ReadKeyframesHermite<ActNodeProperty::eRotation>(
    std::span<const u8> buffer, ActAnimationChannel<ActNodeProperty::eRotation> &channel) -> void {
    const auto num_values_per_keyframe = 13;
    const auto num_keyframes = buffer.size_bytes() / sizeof(f32) / num_values_per_keyframe;

    util::bytes::BinaryReader reader{buffer};
    channel.keyframes.resize(num_keyframes);

    for (uint32_t i = 0; i < num_keyframes; ++i) {
        auto &keyframe = channel.keyframes[i];

        keyframe.time = AssertRead<f32>(reader);
        keyframe.value.x = AssertRead<f32>(reader);
        keyframe.value.y = AssertRead<f32>(reader);
        keyframe.value.z = AssertRead<f32>(reader);
        keyframe.value.w = AssertRead<f32>(reader);
        keyframe.in_dy.x = AssertRead<f32>(reader);
        keyframe.in_dy.y = AssertRead<f32>(reader);
        keyframe.in_dy.z = AssertRead<f32>(reader);
        keyframe.in_dy.w = AssertRead<f32>(reader);
        keyframe.out_dy.x = AssertRead<f32>(reader);
        keyframe.out_dy.y = AssertRead<f32>(reader);
        keyframe.out_dy.z = AssertRead<f32>(reader);
        keyframe.out_dy.w = AssertRead<f32>(reader);
    }
}

auto ReadAnimChannelBlock(std::span<const u8> buffer) -> ActAnyAnimationChannel {
    util::bytes::BinaryReader reader{buffer};

    std::optional<u32> target_node = std::nullopt;
    std::optional<ActNodeProperty> target_prop = std::nullopt;
    std::optional<ActInterpolationMode> interpolation = std::nullopt;
    std::optional<std::string_view> name = std::nullopt;
    std::optional<std::span<const u8>> keyframes_buffer = std::nullopt;

    while (const auto command = ReadCommandType(reader)) {
        switch (*command) {
        case ActCommandType::eAnimChannelSetNode:
            target_node = ReadCommandParameter<u32>(reader);
            break;

        case ActCommandType::eAnimChannelSetProp:
            target_prop = ReadCommandParameterEnum<ActNodeProperty>(reader, ValueToActNodeProperty);
            break;

        case ActCommandType::eAnimChannelSetMode:
            interpolation = ReadCommandParameterEnum<ActInterpolationMode>(reader, ValueToActInterpolationMode);
            break;

        case ActCommandType::eAnimChannelSetName:
            name = ReadCommandParameterString(reader);
            break;

        case ActCommandType::eAnimChannelSetKeyframes:
            keyframes_buffer = ReadCommandParameterBuffer(reader);
            break;

        default:
            throw std::runtime_error{
                fmt::format("invalid animation channel command {:#010x}", static_cast<u32>(*command))};
        }
    }

    if (!target_node.has_value() || !target_prop.has_value() || !interpolation.has_value() ||
        !keyframes_buffer.has_value()) {
        throw std::runtime_error{"animation channel is not complete"};
    }

    std::string resolved_name = name.has_value() ? std::string{name.value()} : "";

    switch (target_prop.value()) {
    case ActNodeProperty::eTranslation: {
        auto channel = ActAnimationChannel<ActNodeProperty::eTranslation>{
            .node_id = target_node.value(),
            .interpolation = interpolation.value(),
            .name = resolved_name,
        };

        if (interpolation == ActInterpolationMode::eHermite) {
            ReadKeyframesHermite(keyframes_buffer.value(), channel);
        } else {
            ReadKeyframes(keyframes_buffer.value(), channel);
        }

        return channel;
    }
    case ActNodeProperty::eRotation: {
        auto channel = ActAnimationChannel<ActNodeProperty::eRotation>{
            .node_id = target_node.value(),
            .interpolation = interpolation.value(),
            .name = resolved_name,
        };

        if (interpolation == ActInterpolationMode::eHermite) {
            ReadKeyframesHermite(keyframes_buffer.value(), channel);
        } else {
            ReadKeyframes(keyframes_buffer.value(), channel);
        }

        return channel;
    }
    case ActNodeProperty::eScale: {
        auto channel = ActAnimationChannel<ActNodeProperty::eScale>{
            .node_id = target_node.value(),
            .interpolation = interpolation.value(),
            .name = resolved_name,
        };

        if (interpolation == ActInterpolationMode::eHermite) {
            ReadKeyframesHermite(keyframes_buffer.value(), channel);
        } else {
            ReadKeyframes(keyframes_buffer.value(), channel);
        }

        return channel;
    }
    case ActNodeProperty::eUvOffset: {
        auto channel = ActAnimationChannel<ActNodeProperty::eUvOffset>{
            .node_id = target_node.value(),
            .interpolation = interpolation.value(),
            .name = resolved_name,
        };

        if (interpolation == ActInterpolationMode::eHermite) {
            ReadKeyframesHermite(keyframes_buffer.value(), channel);
        } else {
            ReadKeyframes(keyframes_buffer.value(), channel);
        }

        return channel;
    }
    case ActNodeProperty::eUvScale: {
        auto channel = ActAnimationChannel<ActNodeProperty::eUvScale>{
            .node_id = target_node.value(),
            .interpolation = interpolation.value(),
            .name = resolved_name,
        };

        if (interpolation == ActInterpolationMode::eHermite) {
            ReadKeyframesHermite(keyframes_buffer.value(), channel);
        } else {
            ReadKeyframes(keyframes_buffer.value(), channel);
        }

        return channel;
    }
    case ActNodeProperty::eAlpha: {
        auto channel = ActAnimationChannel<ActNodeProperty::eAlpha>{
            .node_id = target_node.value(),
            .interpolation = interpolation.value(),
            .name = resolved_name,
        };

        if (interpolation == ActInterpolationMode::eHermite) {
            ReadKeyframesHermite(keyframes_buffer.value(), channel);
        } else {
            ReadKeyframes(keyframes_buffer.value(), channel);
        }

        return channel;
    }
    default:
        break;
    }
}

auto LoadFromBinary(std::span<const u8> buffer) -> ActModel {
    ActModel model;
    util::bytes::BinaryReader reader{buffer};

    const auto magic = AssertRead<u32>(reader);
    const auto version = AssertRead<u32>(reader);
    const auto writer = AssertRead<u32>(reader);
    const auto size = AssertRead<u32>(reader);

    if (kMagicNumber != magic) {
        throw std::runtime_error{"invalid magic number"};
    }

    if (kWriterVersion != version || kWriterSoftware != writer) {
        throw std::runtime_error{"invalid format revision"};
    }

    if (reader.Remaining() < size) {
        throw std::runtime_error{fmt::format("expected remaining {} bytes, got {}", size, reader.Remaining())};
    }

    const auto start_position = reader.Position();
    const auto end_position = start_position + size;

    while (reader.Position() < end_position) {
        const auto block_position = reader.Position();
        const auto block_type = AssertRead<u32>(reader);
        const auto block_size = AssertRead<u32>(reader);

        if (reader.Position() + block_size > end_position) {
            throw std::runtime_error{fmt::format("act: invalid block of size {} at {}", block_size, block_position)};
        }

        const auto block_type_enum = ValueToActBlockType(block_type);
        if (!block_type_enum.has_value()) {
            throw std::runtime_error{
                fmt::format("act: invalid block type {} of block at {}", block_type, block_position)};
        }

        const auto block_buffer = reader.ReadBuffer(block_size);
        if (!block_buffer.has_value()) {
            throw std::runtime_error{
                fmt::format("act: invalid block, expected {} readable bytes in the stream", block_size)};
        }

        switch (block_type_enum.value()) {
        case act::ActBlockType::eImageBlock:
            model.images.emplace_back(ReadImageBlock(block_buffer.value()));
            break;

        case act::ActBlockType::eTextureBlock:
            model.textures.emplace_back(ReadTextureBlock(block_buffer.value()));
            break;

        case act::ActBlockType::eMaterialBlock:
            model.materials.emplace_back(ReadMaterialBlock(block_buffer.value()));
            break;

        case act::ActBlockType::eNodeBlock:
            model.nodes.emplace_back(ReadNodeBlock(block_buffer.value()));
            break;

        case act::ActBlockType::eMeshBlock:
            model.meshes.emplace_back(ReadMeshBlock(block_buffer.value()));
            break;

        case act::ActBlockType::eSubmeshBlock:
            model.submeshes.emplace_back(ReadSubmeshBlock(block_buffer.value()));
            break;

        case act::ActBlockType::eSkinBlock:
            model.skins.emplace_back(ReadSkinBlock(block_buffer.value()));
            break;

        case act::ActBlockType::eSkinNodeBlock:
            model.skin_nodes.emplace_back(ReadSkinNodeBlock(block_buffer.value()));
            break;

        case act::ActBlockType::eAnimationBlock:
            model.animations.emplace_back(ReadAnimationBlock(block_buffer.value()));
            break;

        case act::ActBlockType::eAnimChannelBlock:
            model.animation_channels.emplace_back(ReadAnimChannelBlock(block_buffer.value()));
            break;

        default:
            break;
        }
    }

    return model;
}

auto CheckHeader(std::span<const u8> binary) -> bool {
    BinaryReader reader{binary};

    try {
        u32 magic, software_version, writer_version;
        magic = AssertRead<u32>(reader);
        software_version = AssertRead<u32>(reader);
        writer_version = AssertRead<u32>(reader);

        if (kMagicNumber != magic || kWriterSoftware != software_version || kWriterVersion != writer_version) {
            return false;
        }
    } catch ([[maybe_unused]] const std::exception &e) {
        return false;
    }

    return true;
}

} // namespace act
