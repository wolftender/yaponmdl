#include <array>
#include <stdexcept>

#include <fmt/format.h>

#include "formats/act.hpp"
#include "common/binaryreader.hpp"

#include <glm/gtc/type_ptr.hpp>

namespace act {

using ActParseError = std::runtime_error;

constexpr auto kActByteOrder = util::bytes::BinaryReader::ByteOrder::eLittleEndian;

auto ParseImageMimeType(u32 value) -> std::optional<ImageMimeType> {
    switch (value) {
    case 0x10000001:
        return ImageMimeType::eRawBitmap;
    case 0x10000002:
        return ImageMimeType::ePngBitmap;
    case 0x10000003:
        return ImageMimeType::eJpgBitmap;
    case 0x10000004:
        return ImageMimeType::eTgaBitmap;
    default:
        return std::nullopt;
    }
}

auto ParseTextureWrapType(u32 value) -> std::optional<TextureWrapType> {
    switch (value) {
    case 0x10010001:
        return TextureWrapType::eRepeat;
    case 0x10010002:
        return TextureWrapType::eClampToEdge;
    case 0x10010003:
        return TextureWrapType::eMirroredRepeat;
    default:
        return std::nullopt;
    }
}

auto ParseSubmeshVertexLayout(u32 value) -> std::optional<SubmeshVertexLayout> {
    switch (value) {
    case 0x10020001:
        return SubmeshVertexLayout::eVertexStatic;
    case 0x10020002:
        return SubmeshVertexLayout::eVertexRigged;
    default:
        return std::nullopt;
    }
}

auto ParseAnimInterpolationMode(u32 value) -> std::optional<AnimationInterpolationMode> {
    switch (value) {
    case 0x10030001:
        return AnimationInterpolationMode::eLinear;
    case 0x10030002:
        return AnimationInterpolationMode::eCubicSpline;
    default:
        return std::nullopt;
    }
}

auto ParseAnimPropertyType(u32 value) -> std::optional<AnimationPropertyType> {
    switch (value) {
    case 0x10040001:
        return AnimationPropertyType::eTranslation;
    case 0x10040002:
        return AnimationPropertyType::eRotation;
    case 0x10040003:
        return AnimationPropertyType::eScale;
    default:
        return std::nullopt;
    }
}

auto ParseBlockType(u32 value) -> std::optional<BlockType> {
    switch (value) {
    case 0x20000001:
        return BlockType::eImageBlock;
    case 0x20000002:
        return BlockType::eTextureBlock;
    case 0x20000003:
        return BlockType::eMaterialBlock;
    case 0x20000004:
        return BlockType::eNodeBlock;
    case 0x20000005:
        return BlockType::eMeshBlock;
    case 0x20000006:
        return BlockType::eSubmeshBlock;
    case 0x20000007:
        return BlockType::eSkinBlock;
    case 0x20000008:
        return BlockType::eSkinNodeBlock;
    case 0x20000009:
        return BlockType::eAnimationBlock;
    case 0x2000000a:
        return BlockType::eAnimChannelBlock;
    default:
        return std::nullopt;
    }
}

auto ParseCommandType(u32 value) -> std::optional<CommandType> {
    switch (value) {
    case 0x30010001:
        return CommandType::eImageSetMimeType;
    case 0x30010002:
        return CommandType::eImageSetDimensions;
    case 0x30010003:
        return CommandType::eImageSetBuffer;

    case 0x30020001:
        return CommandType::eTextureSetImage;
    case 0x30020002:
        return CommandType::eTextureSetWrapS;
    case 0x30020003:
        return CommandType::eTextureSetWrapT;

    case 0x30030001:
        return CommandType::eMaterialSetBaseColor;
    case 0x30030002:
        return CommandType::eMaterialSetRoughness;
    case 0x30030003:
        return CommandType::eMaterialSetMetallic;
    case 0x30030004:
        return CommandType::eMaterialSetAlbedoMap;
    case 0x30030005:
        return CommandType::eMaterialSetNormalMap;

    case 0x30040001:
        return CommandType::eNodeSetTranslation;
    case 0x30040002:
        return CommandType::eNodeSetRotation;
    case 0x30040003:
        return CommandType::eNodeSetScale;
    case 0x30040004:
        return CommandType::eNodeSetMesh;
    case 0x30040005:
        return CommandType::eNodeSetSkin;
    case 0x30040006:
        return CommandType::eNodeSetParent;

    case 0x30050001:
        return CommandType::eMeshAddSubmesh;

    case 0x30060001:
        return CommandType::eSubmeshSetLayout;
    case 0x30060002:
        return CommandType::eSubmeshSetVertices;
    case 0x30060003:
        return CommandType::eSubmeshSetIndices;
    case 0x30060004:
        return CommandType::eSubmeshSetMaterial;

    case 0x30070001:
        return CommandType::eSkinAddNode;

    case 0x30080001:
        return CommandType::eSkinNodeSetNode;
    case 0x30080002:
        return CommandType::eSkinNodeSetMatrix;

    case 0x30090001:
        return CommandType::eAnimationAddChannel;

    case 0x300a0001:
        return CommandType::eAnimChannelSetNode;
    case 0x300a0002:
        return CommandType::eAnimChannelSetProp;
    case 0x300a0003:
        return CommandType::eAnimChannelSetMode;
    case 0x300a0004:
        return CommandType::eAnimChannelSetTimeline;
    case 0x300a0005:
        return CommandType::eAnimChannelSetKeyframes;

    default:
        return std::nullopt;
    }
}

template <typename T>
auto AssertRead(util::bytes::BinaryReader &reader, std::optional<std::string_view> message = std::nullopt) -> T {
    const auto value = reader.Read<kActByteOrder, T>();
    if (!value.has_value()) {
        if (message.has_value()) {
            throw ActParseError{std::string{message.value()}};
        } else {
            throw ActParseError{fmt::format("invalid byte read at position {}", reader.Position())};
        }
    }

    return value.value();
}

template <typename T, std::invocable<u32> F>
    requires std::convertible_to<std::invoke_result_t<F, u32>, std::optional<T>>
auto AssertReadEnum(util::bytes::BinaryReader &reader, F parser) -> T {
    const auto raw = AssertRead<u32>(reader);
    const auto enum_value = parser(raw);

    if (!enum_value.has_value()) {
        throw ActParseError{fmt::format("failed to parse act enum type from {}", raw)};
    }

    return enum_value.value();
}

template <typename T, size_t kDim> auto AssertReadVector(util::bytes::BinaryReader &reader) -> glm::vec<kDim, T> {
    glm::vec<kDim, T> result;
    for (uint32_t i = 0; i < kDim; ++i) {
        result[i] = AssertRead<T>(reader);
    }

    return result;
}

template <typename T> auto AssertReadQuat(util::bytes::BinaryReader &reader) -> glm::qua<T> {
    const auto result = reader.ReadQuat<kActByteOrder, T>();
    if (!result.has_value()) {
        throw ActParseError{"failed to read quaternion"};
    }

    return result.value();
}

auto AssertSizedProperty(util::bytes::BinaryReader &reader, u32 size) -> void {
    const auto size_val = AssertRead<u32>(reader);
    if (size_val != size) {
        throw ActParseError{fmt::format("act assertion failure, size {} != {}", size_val, size)};
    }
}

auto ReadCommandType(BinaryReader &reader) -> std::optional<CommandType> {
    const auto command_type = reader.Read<kActByteOrder, u32>();
    if (!command_type.has_value()) {
        return std::nullopt;
    }

    const auto command_type_enum = ParseCommandType(command_type.value());
    return command_type_enum;
}

auto ReadSizedBuffer(BinaryReader &reader) -> std::span<const u8> {
    const auto buffer_size = AssertRead<u32>(reader);
    const auto buffer = reader.ReadBuffer(buffer_size);

    if (!buffer.has_value()) {
        throw ActParseError{fmt::format("failed to read sized buffer of size {}", buffer_size)};
    }

    return buffer.value();
}

auto ParseImageBlock(std::span<const u8> block_buffer) -> Model::Image {
    BinaryReader block_reader{block_buffer};
    Model::Image image{};

    while (const auto command_type = ReadCommandType(block_reader)) {
        switch (*command_type) {
        case act::CommandType::eImageSetMimeType:
            AssertSizedProperty(block_reader, sizeof(u32));
            image.mime_type = AssertReadEnum<ImageMimeType>(block_reader, ParseImageMimeType);
            break;
        case act::CommandType::eImageSetDimensions:
            AssertSizedProperty(block_reader, sizeof(u32) * 2);
            image.dimensions = AssertReadVector<u32, 2>(block_reader);
            break;
        case act::CommandType::eImageSetBuffer: {
            const auto buffer = ReadSizedBuffer(block_reader);
            image.buffer.assign(buffer.begin(), buffer.end());
            break;
        }
        default:
            throw ActParseError{
                fmt::format("act: invalid image command type {}", static_cast<uint32_t>(*command_type))};
        }
    }

    return image;
}

auto ParseTextureBlock(std::span<const u8> block_buffer) -> Model::Texture {
    BinaryReader block_reader{block_buffer};
    Model::Texture texture = {};

    while (auto command_type = ReadCommandType(block_reader)) {
        switch (*command_type) {
        case act::CommandType::eTextureSetWrapS:
            AssertSizedProperty(block_reader, sizeof(u32));
            texture.wrap_s = AssertReadEnum<TextureWrapType>(block_reader, ParseTextureWrapType);
            break;

        case act::CommandType::eTextureSetWrapT:
            AssertSizedProperty(block_reader, sizeof(u32));
            texture.wrap_t = AssertReadEnum<TextureWrapType>(block_reader, ParseTextureWrapType);
            break;

        case act::CommandType::eTextureSetImage:
            AssertSizedProperty(block_reader, sizeof(u32));
            texture.image_id = AssertRead<u32>(block_reader);
            break;

        default:
            throw ActParseError{
                fmt::format("act: invalid texture command type {}", static_cast<uint32_t>(*command_type))};
        }
    }

    return texture;
}

auto ParseMaterialBlock(std::span<const u8> block_buffer) -> Model::Material {
    BinaryReader block_reader{block_buffer};
    Model::Material material = {};

    while (auto command_type = ReadCommandType(block_reader)) {
        switch (*command_type) {
        case act::CommandType::eMaterialSetBaseColor:
            AssertSizedProperty(block_reader, sizeof(f32) * 4);
            material.base_color = AssertReadVector<f32, 4>(block_reader);
            break;

        case act::CommandType::eMaterialSetMetallic:
            AssertSizedProperty(block_reader, sizeof(f32));
            material.metallic = AssertRead<f32>(block_reader);
            break;

        case act::CommandType::eMaterialSetRoughness:
            AssertSizedProperty(block_reader, sizeof(f32));
            material.roughness = AssertRead<f32>(block_reader);
            break;

        case act::CommandType::eMaterialSetAlbedoMap:
            AssertSizedProperty(block_reader, sizeof(u32));
            material.albedo_map_id = AssertRead<u32>(block_reader);
            break;

        case act::CommandType::eMaterialSetNormalMap:
            AssertSizedProperty(block_reader, sizeof(u32));
            material.normal_map_id = AssertRead<u32>(block_reader);
            break;

        default:
            throw ActParseError{
                fmt::format("act: invalid material command type {}", static_cast<uint32_t>(*command_type))};
        }
    }

    return material;
}

auto ParseNodeBlock(std::span<const u8> block_buffer) -> Model::Node {
    BinaryReader block_reader{block_buffer};
    Model::Node node = {};

    node.translation = glm::fvec3{0.0f, 0.0f, 0.0f};
    node.scale = glm::fvec3{1.0f, 1.0f, 1.0f};
    node.rotation = glm::fquat{1.0f, 0.0f, 0.0f, 0.0f}; // this constructor has order WXYZ

    while (auto command_type = ReadCommandType(block_reader)) {
        switch (*command_type) {
        case act::CommandType::eNodeSetMesh:
            AssertSizedProperty(block_reader, sizeof(u32));
            node.mesh_id = AssertRead<u32>(block_reader);
            break;

        case act::CommandType::eNodeSetRotation:
            AssertSizedProperty(block_reader, sizeof(f32) * 4);
            node.rotation = AssertReadQuat<f32>(block_reader);
            break;

        case act::CommandType::eNodeSetScale:
            AssertSizedProperty(block_reader, sizeof(f32) * 3);
            node.scale = AssertReadVector<f32, 3>(block_reader);
            break;

        case act::CommandType::eNodeSetTranslation:
            AssertSizedProperty(block_reader, sizeof(f32) * 3);
            node.translation = AssertReadVector<f32, 3>(block_reader);
            break;

        case act::CommandType::eNodeSetSkin:
            AssertSizedProperty(block_reader, sizeof(u32));
            node.skin_id = AssertRead<u32>(block_reader);
            break;

        case act::CommandType::eNodeSetParent:
            AssertSizedProperty(block_reader, sizeof(u32));
            node.parent_id = AssertRead<u32>(block_reader);
            break;

        default:
            throw ActParseError{fmt::format("act: invalid node command type {}", static_cast<uint32_t>(*command_type))};
        }
    }

    return node;
}

auto ParseMeshBlock(std::span<const u8> block_buffer) -> Model::Mesh {
    BinaryReader block_reader{block_buffer};
    Model::Mesh mesh = {};

    while (auto command_type = ReadCommandType(block_reader)) {
        switch (*command_type) {
        case act::CommandType::eMeshAddSubmesh: {
            u32 submesh;
            AssertSizedProperty(block_reader, sizeof(u32));
            submesh = AssertRead<u32>(block_reader);
            mesh.submesh_ids.emplace_back(submesh);

            break;
        }

        default:
            throw ActParseError{fmt::format("act: invalid mesh command type {}", static_cast<uint32_t>(*command_type))};
        }
    }

    return mesh;
}

auto ParseSubmeshBlock(std::span<const u8> block_buffer) -> Model::AnySubmesh {
    BinaryReader block_reader{block_buffer};

    std::span<const u8> vertex_buffer;
    std::vector<u32> indices;
    act::SubmeshVertexLayout layout;
    u32 material;

    while (auto command_type = ReadCommandType(block_reader)) {
        switch (*command_type) {
        case act::CommandType::eSubmeshSetLayout:
            AssertSizedProperty(block_reader, sizeof(u32));
            layout = AssertReadEnum<SubmeshVertexLayout>(block_reader, ParseSubmeshVertexLayout);
            break;

        case act::CommandType::eSubmeshSetIndices: {
            u32 num_indices;
            num_indices = AssertRead<u32>(block_reader);

            num_indices = num_indices / sizeof(u32);
            indices.reserve(num_indices);
            u32 index;

            for (auto i = 0u; i < num_indices; ++i) {
                index = AssertRead<u32>(block_reader);
                indices.emplace_back(index);
            }

            break;
        }

        case act::CommandType::eSubmeshSetVertices: {
            vertex_buffer = ReadSizedBuffer(block_reader);
            break;
        }

        case act::CommandType::eSubmeshSetMaterial:
            AssertSizedProperty(block_reader, sizeof(u32));
            material = AssertRead<u32>(block_reader);
            break;

        default:
            throw ActParseError{fmt::format("act: invalid mesh command type {}", static_cast<uint32_t>(*command_type))};
        }
    }

    // we already know the vertex type so we can parse it now
    if (SubmeshVertexLayout::eVertexStatic == layout) {
        Model::SubMesh<VertexStatic> submesh = {};

        submesh.indices = std::move(indices);
        submesh.layout = layout;
        submesh.material = material;

        constexpr size_t kBytesPerVertex = 15 * 4; // fields * bytes per field
        const auto num_vertices = vertex_buffer.size() / kBytesPerVertex;

        if (0ull != vertex_buffer.size() % kBytesPerVertex) {
            throw ActParseError{fmt::format("act: invalid vertex buffer size")};
        }

        submesh.vertices.resize(num_vertices);
        BinaryReader vertex_reader{vertex_buffer};

        for (size_t i = 0; i < num_vertices; ++i) {
            submesh.vertices[i].position = AssertReadVector<f32, 3>(vertex_reader);
            submesh.vertices[i].normal = AssertReadVector<f32, 3>(vertex_reader);
            submesh.vertices[i].texcoord = AssertReadVector<f32, 2>(vertex_reader);
            submesh.vertices[i].color = AssertReadVector<f32, 3>(vertex_reader);
            submesh.vertices[i].tangent = AssertReadVector<f32, 4>(vertex_reader);
        }

        return submesh;
    } else if (SubmeshVertexLayout::eVertexRigged == layout) {
        Model::SubMesh<VertexRigged> submesh = {};

        submesh.indices = std::move(indices);
        submesh.layout = layout;
        submesh.material = material;

        constexpr size_t kBytesPerVertex = 23 * 4; // fields * bytes per field
        const auto num_vertices = vertex_buffer.size() / kBytesPerVertex;

        if (0ull != vertex_buffer.size() % kBytesPerVertex) {
            throw ActParseError{fmt::format("act: invalid vertex buffer size")};
        }

        submesh.vertices.resize(num_vertices);
        BinaryReader vertex_reader{vertex_buffer};

        for (size_t i = 0; i < num_vertices; ++i) {
            submesh.vertices[i].position = AssertReadVector<f32, 3>(vertex_reader);
            submesh.vertices[i].normal = AssertReadVector<f32, 3>(vertex_reader);
            submesh.vertices[i].texcoord = AssertReadVector<f32, 2>(vertex_reader);
            submesh.vertices[i].color = AssertReadVector<f32, 3>(vertex_reader);
            submesh.vertices[i].tangent = AssertReadVector<f32, 4>(vertex_reader);
            submesh.vertices[i].joints = AssertReadVector<u32, 4>(vertex_reader);
            submesh.vertices[i].weights = AssertReadVector<f32, 4>(vertex_reader);
        }

        return submesh;
    } else {
        throw ActParseError{fmt::format("act: unsupported vertex layout type")};
    }
}

auto ParseSkinBlock(std::span<const u8> block_buffer) -> Model::Skin {
    BinaryReader block_reader{block_buffer};
    Model::Skin skin = {};

    while (auto command_type = ReadCommandType(block_reader)) {
        switch (*command_type) {
        case act::CommandType::eSkinAddNode: {
            u32 node;
            AssertSizedProperty(block_reader, sizeof(u32));
            node = AssertRead<u32>(block_reader);
            skin.skin_node_ids.emplace_back(node);

            break;
        }

        default:
            throw ActParseError{fmt::format("act: invalid skin command type {}", static_cast<uint32_t>(*command_type))};
        }
    }

    return skin;
}

auto ParseSkinNodeBlock(std::span<const u8> block_buffer) -> Model::SkinNode {
    BinaryReader block_reader{block_buffer};
    Model::SkinNode skin_node = {};

    while (auto command_type = ReadCommandType(block_reader)) {
        switch (*command_type) {
        case act::CommandType::eSkinNodeSetNode:
            AssertSizedProperty(block_reader, sizeof(u32));
            skin_node.node_id = AssertRead<u32>(block_reader);
            break;

        case act::CommandType::eSkinNodeSetMatrix: {
            auto matrix_buffer = ReadSizedBuffer(block_reader);
            if (16 * sizeof(f32) != matrix_buffer.size_bytes()) {
                throw ActParseError{fmt::format("act: matrix buffer has invalid size, expected 16 x f32")};
            }

            BinaryReader matrix_reader{matrix_buffer};
            std::array<f32, 16> matrix_values;
            for (size_t i = 0; i < 16; ++i) {
                matrix_values[i] = AssertRead<f32>(matrix_reader);
            }

            skin_node.inverse_bind_matrix = glm::make_mat4x4(matrix_values.data());
            break;
        }

        default:
            throw ActParseError{
                fmt::format("act: invalid skin node command type {}", static_cast<uint32_t>(*command_type))};
        }
    }

    return skin_node;
}

auto ParseAnimationBlock(std::span<const u8> block_buffer) -> Model::Animation {
    BinaryReader block_reader{block_buffer};
    Model::Animation animation = {};

    while (auto command_type = ReadCommandType(block_reader)) {
        switch (*command_type) {
        case act::CommandType::eAnimationAddChannel: {
            u32 channel;
            AssertSizedProperty(block_reader, sizeof(u32));
            channel = AssertRead<u32>(block_reader);
            animation.channel_ids.emplace_back(channel);

            break;
        }

        default:
            throw ActParseError{
                fmt::format("act: invalid animation command type {}", static_cast<uint32_t>(*command_type))};
        }
    }

    return animation;
}

template <AnimationPropertyType PropertyType>
auto ParseKeyframeData(std::span<const u8> buffer, const std::vector<f32> &timeline)
    -> std::vector<typename Model::AnimationChannel<PropertyType>::Keyframe> {
    using Channel = Model::AnimationChannel<PropertyType>;
    using DataType = Channel::Traits::DataType;

    const auto num_keyframes = timeline.size();
    BinaryReader reader{buffer};
    std::vector<typename Model::AnimationChannel<PropertyType>::Keyframe> keyframes{num_keyframes};

    if constexpr (std::is_same_v<glm::fvec3, DataType>) {
        for (size_t i = 0; i < num_keyframes; ++i) {
            keyframes[i].time = timeline[i];
            keyframes[i].value = AssertReadVector<f32, 3>(reader);
        }
    } else if constexpr (std::is_same_v<glm::fquat, DataType>) {
        for (size_t i = 0; i < num_keyframes; ++i) {
            keyframes[i].time = timeline[i];
            keyframes[i].value = AssertReadQuat<f32>(reader);
        }
    } else {
        static_assert(false, "unsupported keyframe type");
    }

    return keyframes;
}

auto ParseAnimChannelBlock(std::span<const u8> block_buffer) -> Model::AnyAnimationChannel {
    BinaryReader block_reader{block_buffer};

    std::optional<u32> node;
    AnimationPropertyType property;
    AnimationInterpolationMode mode;

    std::vector<f32> timeline;
    std::span<const u8> keyframe_buffer;

    while (auto command_type = ReadCommandType(block_reader)) {
        switch (*command_type) {
        case act::CommandType::eAnimChannelSetProp:
            AssertSizedProperty(block_reader, sizeof(u32));
            property = AssertReadEnum<AnimationPropertyType>(block_reader, ParseAnimPropertyType);
            break;

        case act::CommandType::eAnimChannelSetMode:
            AssertSizedProperty(block_reader, sizeof(u32));
            mode = AssertReadEnum<AnimationInterpolationMode>(block_reader, ParseAnimInterpolationMode);
            break;

        case act::CommandType::eAnimChannelSetNode:
            AssertSizedProperty(block_reader, sizeof(u32));
            node = AssertRead<u32>(block_reader);
            break;

        case act::CommandType::eAnimChannelSetKeyframes: {
            keyframe_buffer = ReadSizedBuffer(block_reader);
            break;
        }

        case act::CommandType::eAnimChannelSetTimeline: {
            u32 num_keyframes;
            num_keyframes = AssertRead<u32>(block_reader);

            num_keyframes = num_keyframes / sizeof(f32);
            timeline.reserve(num_keyframes);
            f32 time;

            for (auto i = 0u; i < num_keyframes; ++i) {
                time = AssertRead<f32>(block_reader);
                timeline.emplace_back(time);
            }

            break;
        }

        default:
            throw ActParseError{
                fmt::format("act: invalid animation channel command type {}", static_cast<uint32_t>(*command_type))};
        }
    }

    if (!node.has_value()) {
        throw ActParseError{fmt::format("act: animation channel requires a node id")};
    }

    switch (property) {
    case act::AnimationPropertyType::eTranslation: {
        auto keyframes = ParseKeyframeData<act::AnimationPropertyType::eTranslation>(keyframe_buffer, timeline);

        Model::AnimationChannel<AnimationPropertyType::eTranslation> channel;
        channel.node_id = node.value();
        channel.interpolation = mode;
        channel.keyframes = std::move(keyframes);

        return channel;
    }

    case act::AnimationPropertyType::eRotation: {
        auto keyframes = ParseKeyframeData<act::AnimationPropertyType::eRotation>(keyframe_buffer, timeline);

        Model::AnimationChannel<AnimationPropertyType::eRotation> channel;
        channel.node_id = node.value();
        channel.interpolation = mode;
        channel.keyframes = std::move(keyframes);

        return channel;
    }

    case act::AnimationPropertyType::eScale: {
        auto keyframes = ParseKeyframeData<act::AnimationPropertyType::eScale>(keyframe_buffer, timeline);

        Model::AnimationChannel<AnimationPropertyType::eScale> channel;
        channel.node_id = node.value();
        channel.interpolation = mode;
        channel.keyframes = std::move(keyframes);

        return channel;
    }

    default:
        break;
    }

    throw ActParseError{"invalid animation channel block"};
}

auto LoadFromBinary(std::span<const u8> binary) -> Model {
    Model model;
    BinaryReader reader{binary};

    u32 magic, software_version, writer_version;
    magic = AssertRead<u32>(reader);
    software_version = AssertRead<u32>(reader);
    writer_version = AssertRead<u32>(reader);

    if (kMagicNumber != magic) {
        throw ActParseError{fmt::format("act: magic number mismatch")};
    }

    if (kWriterSoftware != software_version || kWriterVersion != writer_version) {
        throw ActParseError{fmt::format("act: format version mismatch")};
    }

    u32 model_size;
    model_size = AssertRead<u32>(reader);

    if (reader.Remaining() < model_size) {
        throw ActParseError{fmt::format("act: expected remaining {} bytes, got {}", model_size, reader.Remaining())};
    }

    const auto start_position = reader.Position();
    const auto end_position = start_position + model_size;
    while (reader.Position() < end_position) {
        const auto block_position = reader.Position();

        u32 block_type, block_size;
        block_type = AssertRead<u32>(reader);
        block_size = AssertRead<u32>(reader);

        if (reader.Position() + block_size > end_position) {
            throw ActParseError{fmt::format("act: invalid block of size {} at {}", block_size, block_position)};
        }

        const auto block_type_enum = ParseBlockType(block_type);
        if (!block_type_enum.has_value()) {
            throw ActParseError{fmt::format("act: invalid block type {} of block at {}", block_type, block_position)};
        }

        const auto block_buffer = reader.ReadBuffer(block_size);
        if (!block_buffer.has_value()) {
            throw ActParseError{
                fmt::format("act: invalid block, expected {} readable bytes in the stream", block_size)};
        }

        switch (block_type_enum.value()) {
        case act::BlockType::eImageBlock:
            model.images.emplace_back(ParseImageBlock(block_buffer.value()));
            break;

        case act::BlockType::eTextureBlock:
            model.textures.emplace_back(ParseTextureBlock(block_buffer.value()));
            break;

        case act::BlockType::eMaterialBlock:
            model.materials.emplace_back(ParseMaterialBlock(block_buffer.value()));
            break;

        case act::BlockType::eNodeBlock:
            model.nodes.emplace_back(ParseNodeBlock(block_buffer.value()));
            break;

        case act::BlockType::eMeshBlock:
            model.meshes.emplace_back(ParseMeshBlock(block_buffer.value()));
            break;

        case act::BlockType::eSubmeshBlock:
            model.submeshes.emplace_back(ParseSubmeshBlock(block_buffer.value()));
            break;

        case act::BlockType::eSkinBlock:
            model.skins.emplace_back(ParseSkinBlock(block_buffer.value()));
            break;

        case act::BlockType::eSkinNodeBlock:
            model.skin_nodes.emplace_back(ParseSkinNodeBlock(block_buffer.value()));
            break;

        case act::BlockType::eAnimationBlock:
            model.animations.emplace_back(ParseAnimationBlock(block_buffer.value()));
            break;

        case act::BlockType::eAnimChannelBlock:
            model.animation_channels.emplace_back(ParseAnimChannelBlock(block_buffer.value()));
            break;

        default:
            // LogWarning("act: skipping block of type {}, size {} bytes", block_type, block_size);
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