#include <stdexcept>
#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>

#include "formats/gmo.hpp"
#include "formats/gmodef.hpp"

#include "common/binaryreader.hpp"

namespace gmo {

#ifndef NDEBUG
#define GMO_DEBUG_PRINT(f, ...) fmt::println(stdout, f, __VA_ARGS__)
#else
#define GMO_DEBUG_PRINT(fmt, ...)
#endif

using GmoParseError = std::runtime_error;

/**
 * @brief GMO file header structure
 */
struct GmoHeader {
    uint32_t signature;
    uint32_t version;
    uint32_t style;
    uint32_t option;
};

/**
 * @brief GMO binary file chunk structure
 *
 * This structure holds information about binary chunks in the file
 * It can be used to retrieve data from the file
 */
struct GmoChunk {
    uint64_t position;

    uint16_t type;
    uint16_t args_offset;
    uint32_t next_offset;
    uint32_t child_offset;
    uint32_t data_offset;

    uint32_t self_id;
    std::optional<uint32_t> next_id;
    std::optional<uint32_t> child_id;

    std::span<uint8_t> data;
};

constexpr uint32_t kChunkHeaderSize =
    sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t);

/**
 * @brief Simple function to help with inline binary reads
 *
 * @tparam T data type to read from the stream
 * @param reader reader
 * @param message message to add to the exception if read fails
 * @return T value retrieved from the binary buffer
 */
template <typename T>
auto AssertRead(util::bytes::BinaryReader &reader, std::optional<std::string_view> message = std::nullopt) -> T {
    const auto value = reader.Read<T>();
    if (!value.has_value()) {
        if (message.has_value()) {
            throw GmoParseError{std::string{message.value()}};
        } else {
            throw GmoParseError{fmt::format("invalid byte read at position {}", reader.Position())};
        }
    }

    return value.value();
}

auto AssertSeek(util::bytes::BinaryReader &reader, uint64_t position) -> void {
    const auto res = reader.Seek(position);
    if (util::bytes::BinaryReader::Result::eSuccess != res) {
        throw GmoParseError{fmt::format("invalid gmo offset calculated {}", position)};
    }
}

/**
 * @brief Get the Gmo Header object
 *
 * @param reader
 * @return GmoHeader
 */
auto GetGmoHeader(util::bytes::BinaryReader &reader) -> GmoHeader {
    GmoHeader header = {};
    header.signature = AssertRead<uint32_t>(reader, "invalid header");
    header.version = AssertRead<uint32_t>(reader, "invalid header");
    header.style = AssertRead<uint32_t>(reader, "invalid header");
    header.option = AssertRead<uint32_t>(reader, "invalid header");

    if (header.signature != kSceGmoFormatSignature) {
        throw GmoParseError{"incorrect file signature"};
    }

    if (header.version != kSceGmoFormatVersion) {
        throw GmoParseError{"incorrect file signature"};
    }

    if (header.style != kSceGmoFormatPsp) {
        throw GmoParseError{"incorrect format style, expected psp"};
    }

    GMO_DEBUG_PRINT(
        "gmo header check: \n\tsignature:\t{:#010x}\n\tversion:\t{:#010x}\n\tstyle:\t\t{:#010x}\n\toption:\t\t{:#010x}",
        header.signature, header.version, header.style, header.option);

    return header;
}

inline auto GetChunkType(const GmoChunk &chunk) -> uint32_t { return (~SCEGMO_HALF_CHUNK & chunk.type); }
inline auto GetChunkNextPosition(const GmoChunk &chunk) -> uint64_t { return chunk.position + chunk.next_offset; }
inline auto GetChunkSize(const GmoChunk &chunk) -> uint32_t { return chunk.next_offset; }

inline auto GetChunkChildPosition(const GmoChunk &chunk) -> uint64_t {
    if (SCEGMO_HALF_CHUNK & chunk.type) {
        return GetChunkNextPosition(chunk);
    }

    return chunk.position + chunk.child_offset;
}

inline auto GetChunkArgsOffset(const GmoChunk &chunk) -> uint32_t {
    if (SCEGMO_HALF_CHUNK & chunk.type) {
        return chunk.position + 8;
    }

    return chunk.position + chunk.args_offset;
}

inline auto GetChunkArgsSize(const GmoChunk &chunk) -> uint32_t {
    if (SCEGMO_HALF_CHUNK & chunk.type) {
        return chunk.next_offset - 8;
    }

    return chunk.data_offset - chunk.args_offset;
}

inline auto GetChunkDataOffset(const GmoChunk &chunk) -> uint32_t {
    if (SCEGMO_HALF_CHUNK & chunk.type) {
        return GetChunkArgsOffset(chunk);
    }

    return chunk.position + chunk.data_offset;
}

inline auto GetChunkDataSize(const GmoChunk &chunk) -> uint32_t {
    if (SCEGMO_HALF_CHUNK & chunk.type) {
        return 0;
    }

    return chunk.child_offset - chunk.data_offset;
}

inline auto GetChunkNameOffset(const GmoChunk &chunk) -> std::optional<uint32_t> {
    if (SCEGMO_HALF_CHUNK & chunk.type) {
        return std::nullopt;
    }

    return chunk.position + kChunkHeaderSize + 1;
}

inline auto GetChunkNameSize(const GmoChunk &chunk) -> uint32_t {
    if (SCEGMO_HALF_CHUNK & chunk.type) {
        return 0;
    }

    return chunk.args_offset - 16;
}

inline auto RefType(uint32_t ref) { return (0x7fff & ((ref) >> 16)); }
inline auto RefLevel(uint32_t ref) { return (0x000f & ((ref) >> 12)); }
inline auto RefIndex(uint32_t ref) { return (0x0fff & (ref)); }

/**
 * @brief Read binary chunk at the current position
 *
 * @param reader binary reader
 * @return GmoChunk decoded chunk structure
 */
auto ReadChunk(util::bytes::BinaryReader &reader) -> GmoChunk {
    GmoChunk chunk = {};
    chunk.position = reader.Position();
    chunk.type = AssertRead<uint16_t>(reader, "invalid chunk");
    chunk.args_offset = AssertRead<uint16_t>(reader, "invalid chunk");
    chunk.next_offset = AssertRead<uint32_t>(reader, "invalid chunk");
    chunk.child_offset = AssertRead<uint32_t>(reader, "invalid chunk");
    chunk.data_offset = AssertRead<uint32_t>(reader, "invalid chunk");

    // for now, don't initialize those, they will be initialized later
    chunk.self_id = 0;
    chunk.next_id = std::nullopt;
    chunk.child_id = std::nullopt;

    return chunk;
}

auto ReadMat4(util::bytes::BinaryReader &reader) -> glm::fmat4x4 {
    std::array<float, 16> values;
    for (uint32_t j = 0; j < 16; ++j) {
        values[j] = AssertRead<float>(reader);
    }

    return glm::make_mat4(values.data());
}

/**
 * @brief Retrieves the total count of children chunks in the given GMO chunk
 *
 * @param parent_reader binary reader for parent chunk
 * @param parent_chunk parent chunk structure obtained from gmo::ReadChunk
 * @return uint32_t number of children chunk (total)
 */
auto CountChunksRecursive(const util::bytes::BinaryReader &parent_reader, const GmoChunk &parent_chunk) -> uint32_t {
    const auto end_position = GetChunkNextPosition(parent_chunk);
    uint32_t result = 0;

    auto reader = parent_reader;
    auto current_position = GetChunkChildPosition(parent_chunk);

    while (current_position < end_position) {
        AssertSeek(reader, current_position);

        const auto current_chunk = ReadChunk(reader);
        const auto next_position = GetChunkNextPosition(current_chunk);

        result = result + CountChunksRecursive(reader, current_chunk) + 1; // add number of children

        if (next_position <= current_position) {
            break;
        }

        current_position = next_position;
    }

    return result;
}

/**
 * @brief Retrieves the total count of chunks in given GMO buffer
 *
 * @param parent_reader binary reader for the parent chunk
 * @return uint32_t number of chunks in the file
 */
auto CountChunks(const util::bytes::BinaryReader &parent_reader) -> uint32_t {
    uint32_t result = 0;
    auto reader = parent_reader;

    const auto start_position = parent_reader.Position();
    const auto end_position = start_position + parent_reader.Remaining();
    auto current_position = start_position;

    while (current_position < end_position) {
        AssertSeek(reader, current_position);

        const auto current_chunk = ReadChunk(reader);
        const auto next_position = GetChunkNextPosition(current_chunk);

        if (next_position <= current_position) {
            throw GmoParseError{"gmo file has a looping reference between chunks"};
        }

        result = result + CountChunksRecursive(reader, current_chunk) + 1;
        current_position = next_position;
    }

    return result;
}

/**
 * @brief Loader context for the GMO file
 *
 * This class will map the entire file and then perform loading operations
 * on the "flattened" file structure as requested. This way the operations should
 * not be architecture or endianness dependent, as everything is abstracted away
 * by primitives like binary reader
 */
class GmoLoaderContext final {
public:
    GmoLoaderContext(std::span<const uint8_t> buffer) : buffer_{buffer} {
        util::bytes::BinaryReader reader{buffer};
        (void)GetGmoHeader(reader);

        // realistically, we don't need to count the chunks
        // but it is done here anyways to validate the file
        // also this allows to reduce the number of reallocs
        const auto num_chunks = CountChunks(reader);

        GMO_DEBUG_PRINT("detected gmo structure with {} chunks", num_chunks);
        map_.reserve(num_chunks);

        // initialize the chunk tree
        InitializeChunks(reader);

        // sanity check: the top level tags should be SCEGMO_FILEs
        std::optional<uint32_t> current_id = 0;
        while (current_id.has_value()) {
            if (GetChunkType(map_[current_id.value()]) != SCEGMO_FILE) {
                throw GmoParseError{"invalid top-level chunk"};
            }

            current_id = map_[current_id.value()].next_id;
        }
    }

    auto LoadAllModels() const -> std::vector<GmoModel> {
        std::vector<GmoModel> result;

        const auto &file_node = map_.front();
        auto child_id = file_node.child_id;

        while (child_id.has_value()) {
            const auto &child = map_[child_id.value()];
            result.emplace_back(LoadModel(child));
            child_id = child.next_id;
        }

        return result;
    }

    GmoLoaderContext(const GmoLoaderContext &) = delete;
    auto operator=(const GmoLoaderContext &) = delete;

    GmoLoaderContext(GmoLoaderContext &&) = delete;
    auto operator=(GmoLoaderContext &&) = delete;

private:
    /**
     * @brief Load a bone from given chunk
     *
     * @param bone_chunk the chunk with bone data
     * @return GmoBone bone structure
     */
    auto LoadBone(const GmoChunk &bone_chunk) const -> GmoBone {
        const glm::fvec3 kAxisX = {1.0f, 0.0f, 0.0f};
        const glm::fvec3 kAxisY = {0.0f, 1.0f, 0.0f};
        const glm::fvec3 kAxisZ = {0.0f, 0.0f, 1.0f};

        util::bytes::BinaryReader reader{buffer_};

        if (GetChunkType(bone_chunk) != SCEGMO_BONE) {
            throw GmoParseError{"cannot load bone from non-bone chunk"};
        }

        const auto num_draw_parts = CountChildrenOfType(bone_chunk, SCEGMO_DRAW_PART);

        GmoBone bone;
        bone.draw_parts.reserve(num_draw_parts);

        auto current_id = bone_chunk.child_id;
        while (current_id.has_value()) {
            const auto &chunk = map_[current_id.value()];
            const auto type = GetChunkType(chunk);
            const auto args_offset = GetChunkArgsOffset(chunk);

            switch (type) {
            case SCEGMO_PARENT_BONE: {
                AssertSeek(reader, args_offset);
                bone.parent_bone = RefIndex(AssertRead<uint32_t>(reader));
                break;
            }

            case SCEGMO_VISIBILITY: {
                AssertSeek(reader, args_offset);
                bone.visibility = AssertRead<uint32_t>(reader);
                bone.flags = bone.flags | GmoBoneFlags::eBoneHasVisibility;
                break;
            }

            case SCEGMO_MORPH_WEIGHTS: {
                throw GmoParseError{"SCEGMO_MORPH_WEIGHTS is unsupported"};
            }

            case SCEGMO_MORPH_INDEX: {
                throw GmoParseError{"SCEGMO_MORPH_INDEX is unsupported"};
            }

            case SCEGMO_BLEND_BONES: {
                AssertSeek(reader, args_offset);
                const auto num_bones = AssertRead<uint32_t>(reader);

                if (num_bones > 8) {
                    throw GmoParseError{fmt::format("cannot have more than 8 blend bones, requested: {}", num_bones)};
                }

                bone.blend_bones.reserve(num_bones);
                for (uint32_t i = 0; i < num_bones; ++i) {
                    bone.blend_bones.emplace_back(RefIndex(AssertRead<uint32_t>(reader)));
                }

                break;
            }

            case SCEGMO_BLEND_OFFSETS: {
                AssertSeek(reader, args_offset);
                const auto num_offsets = AssertRead<uint32_t>(reader);

                if (num_offsets > 8) {
                    throw GmoParseError{
                        fmt::format("cannot have more than 8 blend offsets, requested: {}", num_offsets)};
                }

                bone.blend_offsets.resize(num_offsets);
                for (uint32_t i = 0; i < num_offsets; ++i) {
                    bone.blend_offsets[i] = ReadMat4(reader);
                }

                break;
            }

            case SCEGMO_PIVOT: {
                AssertSeek(reader, args_offset);
                bone.pivot.x = AssertRead<float>(reader);
                bone.pivot.y = AssertRead<float>(reader);
                bone.pivot.z = AssertRead<float>(reader);
                bone.flags = bone.flags | GmoBoneFlags::eBoneHasPivot;
                break;
            }

            case SCEGMO_MULT_MATRIX: {
                AssertSeek(reader, args_offset);
                bone.local_matrix = ReadMat4(reader);
                bone.flags = bone.flags | GmoBoneFlags::eBoneHasMultMatrix;
                break;
            }

            case SCEGMO_TRANSLATE: {
                AssertSeek(reader, args_offset);
                bone.translation.x = AssertRead<float>(reader);
                bone.translation.y = AssertRead<float>(reader);
                bone.translation.z = AssertRead<float>(reader);
                bone.flags = bone.flags | GmoBoneFlags::eBoneHasTranslation;
                break;
            }

            case SCEGMO_ROTATE_ZYX: {
                AssertSeek(reader, args_offset);
                const auto rz = AssertRead<float>(reader);
                const auto ry = AssertRead<float>(reader);
                const auto rx = AssertRead<float>(reader);
                bone.rotation = glm::angleAxis(rz, kAxisZ) * glm::angleAxis(ry, kAxisY) * glm::angleAxis(rx, kAxisX);
                bone.flags = bone.flags | GmoBoneFlags::eBoneHasRotation;
                break;
            }

            case SCEGMO_ROTATE_YXZ: {
                AssertSeek(reader, args_offset);
                const auto ry = AssertRead<float>(reader);
                const auto rx = AssertRead<float>(reader);
                const auto rz = AssertRead<float>(reader);
                bone.rotation = glm::angleAxis(ry, kAxisY) * glm::angleAxis(rx, kAxisX) * glm::angleAxis(rz, kAxisZ);
                bone.flags = bone.flags | GmoBoneFlags::eBoneHasRotation;
                break;
            }

            case SCEGMO_ROTATE_Q: {
                AssertSeek(reader, args_offset);
                const auto x = AssertRead<float>(reader);
                const auto y = AssertRead<float>(reader);
                const auto z = AssertRead<float>(reader);
                const auto w = AssertRead<float>(reader);
                bone.rotation = glm::fquat{w, x, y, z};
                bone.flags = bone.flags | GmoBoneFlags::eBoneHasRotation;
                break;
            }

            case SCEGMO_SCALE: {
                AssertSeek(reader, args_offset);
                bone.scale.x = AssertRead<float>(reader);
                bone.scale.y = AssertRead<float>(reader);
                bone.scale.z = AssertRead<float>(reader);
                bone.flags = bone.flags | GmoBoneFlags::eBoneHasScale1;
                break;
            }

            case SCEGMO_SCALE_2: {
                AssertSeek(reader, args_offset);
                bone.scale.x = AssertRead<float>(reader);
                bone.scale.y = AssertRead<float>(reader);
                bone.scale.z = AssertRead<float>(reader);
                bone.flags = bone.flags | GmoBoneFlags::eBoneHasScale2;
                break;
            }

            case SCEGMO_SCALE_3: {
                AssertSeek(reader, args_offset);
                bone.scale.x = AssertRead<float>(reader);
                bone.scale.y = AssertRead<float>(reader);
                bone.scale.z = AssertRead<float>(reader);
                bone.flags = bone.flags | GmoBoneFlags::eBoneHasScale3;
                break;
            }

            case SCEGMO_DRAW_PART: {
                AssertSeek(reader, args_offset);
                bone.draw_parts.emplace_back(RefIndex(AssertRead<uint32_t>(reader)));
                break;
            }

            case SCEGMO_BOUNDING_BOX: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                bone.bounding_min.x = AssertRead<float>(reader);
                bone.bounding_min.y = AssertRead<float>(reader);
                bone.bounding_min.z = AssertRead<float>(reader);
                bone.bounding_max.x = AssertRead<float>(reader);
                bone.bounding_max.y = AssertRead<float>(reader);
                bone.bounding_max.z = AssertRead<float>(reader);
                bone.flags = bone.flags | GmoBoneFlags::eBoneHasBoundingBox;
                break;
            }

            case 226: {
                // TODO: zsort is unk0 == 0
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                const auto unknown0 = AssertRead<uint32_t>(reader);
                const auto unknown1 = AssertRead<uint32_t>(reader);

                GMO_DEBUG_PRINT("type 226, unknown0 = {}, unknown1 = {}", unknown0, unknown1);
                break;
            }

            default:
                GMO_DEBUG_PRINT("skipping chunk type for bone: {}", type);
                break;
            }

            // if not assigned set to identity matrices
            if (bone.blend_bones.size() > 0 && bone.blend_offsets.size() != bone.blend_bones.size()) {
                bone.blend_offsets.resize(bone.blend_bones.size(), glm::fmat4x4{1.0f});
            }

            current_id = chunk.next_id;
        }

        return bone;
    }

    auto LoadMesh(const GmoChunk &mesh_chunk) const -> GmoMesh {
        util::bytes::BinaryReader reader{buffer_};

        if (GetChunkType(mesh_chunk) != SCEGMO_MESH) {
            throw GmoParseError{"cannot load mesh from non-mesh chunk"};
        }

        GmoMesh mesh;

        auto current_id = mesh_chunk.child_id;
        while (current_id.has_value()) {
            const auto &chunk = map_[current_id.value()];
            const auto type = GetChunkType(chunk);

            switch (type) {
            case SCEGMO_SET_MATERIAL: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                mesh.material = RefIndex(AssertRead<uint32_t>(reader));
                break;
            }

            case SCEGMO_BLEND_SUBSET: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                const auto num_indices = AssertRead<uint32_t>(reader);

                mesh.blend_subset.reserve(num_indices);
                for (uint32_t i = 0; i < num_indices; ++i) {
                    mesh.blend_subset.emplace_back(AssertRead<uint32_t>(reader));
                }

                break;
            }

            case SCEGMO_SUBDIVISION: {
                throw GmoParseError{fmt::format("SCEGMO_SUBDIVISION is unsupported")};
            }

            case SCEGMO_KNOT_VECTOR_U: {
                throw GmoParseError{fmt::format("SCEGMO_KNOT_VECTOR_U is unsupported")};
            }

            case SCEGMO_KNOT_VECTOR_V: {
                throw GmoParseError{fmt::format("SCEGMO_KNOT_VECTOR_V is unsupported")};
            }

            case SCEGMO_DRAW_PARTICLE: {
                throw GmoParseError{fmt::format("SCEGMO_DRAW_PARTICLE is unsupported")};
            }

            case SCEGMO_DRAW_ARRAYS: {
                break;
            }

            case SCEGMO_DRAW_B_SPLINE: {
                break;
            }

            case SCEGMO_DRAW_RECT_MESH: {
                break;
            }

            case SCEGMO_DRAW_RECT_PATCH: {
                break;
            }

            default:
                GMO_DEBUG_PRINT("skipping chunk type for mesh: {}", type);
                break;
            }

            current_id = chunk.next_id;
        }

        return mesh;
    }

    /**
     * @brief Load a part from chunk data
     *
     * @param part_chunk the chunk that contains part data
     * @return GmoPart loaded part
     */
    auto LoadPart(const GmoChunk &part_chunk) const -> GmoPart {
        util::bytes::BinaryReader reader{buffer_};

        if (GetChunkType(part_chunk) != SCEGMO_PART) {
            throw GmoParseError{"cannot load part from non-part chunk"};
        }

        GmoPart part;

        auto current_id = part_chunk.child_id;
        while (current_id.has_value()) {
            const auto &chunk = map_[current_id.value()];
            const auto type = GetChunkType(chunk);

            switch (type) {
            case SCEGMO_MESH:
                break;

            case SCEGMO_ARRAYS:
                break;

            case SCEGMO_BOUNDING_BOX:
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                part.bounding_min.x = AssertRead<float>(reader);
                part.bounding_min.y = AssertRead<float>(reader);
                part.bounding_min.z = AssertRead<float>(reader);
                part.bounding_max.x = AssertRead<float>(reader);
                part.bounding_max.y = AssertRead<float>(reader);
                part.bounding_max.z = AssertRead<float>(reader);
                break;

            default:
                GMO_DEBUG_PRINT("skipping chunk type for part: {}", type);
                break;
            }

            current_id = chunk.next_id;
        }

        return part;
    }

    auto LoadModel(const GmoChunk &model_chunk) const -> GmoModel {
        util::bytes::BinaryReader reader{buffer_};

        if (GetChunkType(model_chunk) != SCEGMO_MODEL) {
            throw GmoParseError{"cannot load model from non-model chunk"};
        }

        const auto num_bone_chunks = CountChildrenOfType(model_chunk, SCEGMO_BONE);
        const auto num_part_chunks = CountChildrenOfType(model_chunk, SCEGMO_PART);
        const auto num_material_chunks = CountChildrenOfType(model_chunk, SCEGMO_MATERIAL);
        const auto num_texture_chunks = CountChildrenOfType(model_chunk, SCEGMO_TEXTURE);
        const auto num_motion_chunks = CountChildrenOfType(model_chunk, SCEGMO_MOTION);

        GMO_DEBUG_PRINT(
            "validated gmo "
            "model:\n\tbones:\t\t{}\n\tparts:\t\t{}\n\tmaterials:\t{}\n\ttextures:\t{}\n\tmotions:\t{}",
            num_bone_chunks, num_part_chunks, num_material_chunks, num_texture_chunks, num_motion_chunks);

        GmoModel model;
        model.bones.reserve(num_bone_chunks);
        model.parts.reserve(num_part_chunks);
        model.materials.reserve(num_material_chunks);
        model.textures.reserve(num_texture_chunks);
        model.motions.reserve(num_motion_chunks);

        auto current_id = model_chunk.child_id;
        while (current_id.has_value()) {
            const auto &chunk = map_[current_id.value()];
            const auto type = GetChunkType(chunk);

            switch (type) {
            case SCEGMO_BONE:
                model.bones.emplace_back(LoadBone(chunk));
                break;

            case SCEGMO_PART:
                model.parts.emplace_back(LoadPart(chunk));
                break;

            case SCEGMO_MATERIAL:
                break;

            case SCEGMO_TEXTURE:
                break;

            case SCEGMO_MOTION:
                break;

            case SCEGMO_BOUNDING_BOX:
                model.flags = model.flags | GmoModelFlags::eModelHasBoundingBox;
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                model.bounding_min.x = AssertRead<float>(reader);
                model.bounding_min.y = AssertRead<float>(reader);
                model.bounding_min.z = AssertRead<float>(reader);
                model.bounding_max.x = AssertRead<float>(reader);
                model.bounding_max.y = AssertRead<float>(reader);
                model.bounding_max.z = AssertRead<float>(reader);
                break;

            case SCEGMO_VERTEX_OFFSET:
                throw GmoParseError{fmt::format("SCEGMO_VERTEX_OFFSET is unsupported")};

            default:
                GMO_DEBUG_PRINT("skipping chunk type for model: {}", type);
                break;
            }

            current_id = chunk.next_id;
        }

        return model;
    }

    auto CountChildrenOfType(const GmoChunk &chunk, uint16_t type) const -> uint32_t {
        uint32_t count = 0;
        auto current_id = chunk.child_id;
        while (current_id.has_value()) {
            const auto &node = map_[current_id.value()];
            if (GetChunkType(node) == type) {
                count++;
            }

            current_id = node.next_id;
        }

        return count;
    }

    /**
     * @brief Insert a new chunk into the map
     *
     * @param chunk chunk structure
     * @return uint32_t new chunks id
     */
    auto InsertChunk(GmoChunk &&chunk) -> uint32_t {
        map_.emplace_back(std::move(chunk));
        return static_cast<uint32_t>(map_.size() - 1);
    }

    /**
     * @brief Recursively initialize the GMO tree
     *
     * @param parent_reader parent chunk reader
     * @param parent_id parent node id
     */
    auto InitializeChunksRecursive(const util::bytes::BinaryReader &parent_reader, uint32_t parent_id) -> void {
        const auto end_position = GetChunkNextPosition(map_[parent_id]);

        auto reader = parent_reader;
        auto current_position = GetChunkChildPosition(map_[parent_id]);

        std::optional<uint32_t> prev_chunk_id = std::nullopt;

        while (current_position < end_position) {
            AssertSeek(reader, current_position);

            const auto current_chunk_id = InsertChunk(ReadChunk(reader));
            const auto next_position = GetChunkNextPosition(map_[current_chunk_id]);

            if (!map_[parent_id].child_id.has_value()) {
                map_[parent_id].child_id = current_chunk_id;
            }

            map_[current_chunk_id].self_id = current_chunk_id;
            if (prev_chunk_id.has_value()) {
                map_[prev_chunk_id.value()].next_id = current_chunk_id;
            }

            InitializeChunksRecursive(reader, current_chunk_id);

            if (next_position <= current_position) {
                return;
            }

            current_position = next_position;
            prev_chunk_id = current_chunk_id;
        }
    }

    /**
     * @brief Initialize the chunk tree
     * GMO chunks are organized as a "tree" of sorts, to not juggle pointers around
     * we make a nice array-tree out of them
     *
     * @param parent_reader parent binary reader
     */
    auto InitializeChunks(const util::bytes::BinaryReader &parent_reader) -> void {
        auto reader = parent_reader;

        const auto start_position = parent_reader.Position();
        const auto end_position = start_position + parent_reader.Remaining();
        auto current_position = start_position;

        std::optional<uint32_t> prev_chunk_id = std::nullopt;

        while (current_position < end_position) {
            AssertSeek(reader, current_position);

            const auto current_chunk_id = InsertChunk(ReadChunk(reader));
            const auto next_position = GetChunkNextPosition(map_[current_chunk_id]);

            map_[current_chunk_id].self_id = current_chunk_id;
            if (prev_chunk_id.has_value()) {
                map_[prev_chunk_id.value()].next_id = current_chunk_id;
            }

            if (next_position <= current_position) {
                throw GmoParseError{"gmo file has a looping reference between chunks"};
            }

            InitializeChunksRecursive(reader, current_chunk_id);

            current_position = next_position;
            prev_chunk_id = current_chunk_id;
        }
    }

    std::span<const uint8_t> buffer_;
    std::vector<GmoChunk> map_;
};

std::vector<GmoModel> LoadModelFromMemory(std::span<const uint8_t> buffer) {
    GmoLoaderContext context{buffer};
    return context.LoadAllModels();
}

} // namespace gmo
