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

    std::string name;
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

    return chunk.position + kChunkHeaderSize;
}

inline auto GetChunkNameSize(const GmoChunk &chunk) -> uint32_t {
    if (SCEGMO_HALF_CHUNK & chunk.type) {
        return 0;
    }

    return chunk.args_offset - 16;
}

// clang-format off
enum SceGuFmtTexture {
    eSceGuFmtTextureNONE    = 0,
    eSceGuFmtTextureUBYTE   = 1,
    eSceGuFmtTextureUSHORT  = 2,
    eSceGuFmtTextureFLOAT   = 3,
};

enum SceGuFmtColor {
    eSceGuFmtColorNONE      = 0,
    eSceGuFmtColorPF5650    = 4,
    eSceGuFmtColorPF5551    = 5,
    eSceGuFmtColorPF4444    = 6,
    eSceGuFmtColorPF8888    = 7
};

enum SceGuFmtNormal {
    eSceGuFmtNormalNONE     = 0,
    eSceGuFmtNormalBYTE     = 1,
    eSceGuFmtNormalSHORT    = 2,
    eSceGuFmtNormalFLOAT    = 3,
};

enum SceGuFmtVertex {
    eSceGuFmtVertexNONE     = 0,
    eSceGuFmtVertexBYTE     = 1,
    eSceGuFmtVertexSHORT    = 2,
    eSceGuFmtVertexFLOAT    = 3,
};

enum SceGuFmtWeight {
    eSceGuFmtWeightNONE     = 0,
    eSceGuFmtWeightUBYTE    = 1,
    eSceGuFmtWeightUSHORT   = 2,
    eSceGuFmtWeightFLOAT    = 3,
};

enum SceGuFmtIndex {
    eSceGuFmtIndexNONE      = 0,
    eSceGuFmtIndexUBYTE     = 1,
    eSceGuFmtIndexUSHORT    = 2,
};
// clang-format on

constexpr auto ToString(SceGuFmtTexture v) -> const char * {
    switch (v) {
    case eSceGuFmtTextureNONE:
        return "NONE";
    case eSceGuFmtTextureUBYTE:
        return "UBYTE";
    case eSceGuFmtTextureUSHORT:
        return "USHORT";
    case eSceGuFmtTextureFLOAT:
        return "FLOAT";
    default:
        return "UNKNOWN";
    }
}

constexpr auto ToString(SceGuFmtColor v) -> const char * {
    switch (v) {
    case eSceGuFmtColorNONE:
        return "NONE";
    case eSceGuFmtColorPF5650:
        return "PF5650";
    case eSceGuFmtColorPF5551:
        return "PF5551";
    case eSceGuFmtColorPF4444:
        return "PF4444";
    case eSceGuFmtColorPF8888:
        return "PF8888";
    default:
        return "UNKNOWN";
    }
}

constexpr auto ToString(SceGuFmtNormal v) -> const char * {
    switch (v) {
    case eSceGuFmtNormalNONE:
        return "NONE";
    case eSceGuFmtNormalBYTE:
        return "BYTE";
    case eSceGuFmtNormalSHORT:
        return "SHORT";
    case eSceGuFmtNormalFLOAT:
        return "FLOAT";
    default:
        return "UNKNOWN";
    }
}

constexpr auto ToString(SceGuFmtVertex v) -> const char * {
    switch (v) {
    case eSceGuFmtVertexNONE:
        return "NONE";
    case eSceGuFmtVertexBYTE:
        return "BYTE";
    case eSceGuFmtVertexSHORT:
        return "SHORT";
    case eSceGuFmtVertexFLOAT:
        return "FLOAT";
    default:
        return "UNKNOWN";
    }
}

constexpr auto ToString(SceGuFmtWeight v) -> const char * {
    switch (v) {
    case eSceGuFmtWeightNONE:
        return "NONE";
    case eSceGuFmtWeightUBYTE:
        return "UBYTE";
    case eSceGuFmtWeightUSHORT:
        return "USHORT";
    case eSceGuFmtWeightFLOAT:
        return "FLOAT";
    default:
        return "UNKNOWN";
    }
}

constexpr auto ToString(SceGuFmtIndex v) -> const char * {
    switch (v) {
    case eSceGuFmtIndexNONE:
        return "NONE";
    case eSceGuFmtIndexUBYTE:
        return "UBYTE";
    case eSceGuFmtIndexUSHORT:
        return "USHORT";
    default:
        return "UNKNOWN";
    }
}

inline auto ColorPF5650ToRGBA8(uint16_t color) -> glm::fvec4 {
    const auto r = uint16_t{0b1111100000000000} & color;
    const auto g = uint16_t{0b0000011111100000} & color;
    const auto b = uint16_t{0b0000000000011111} & color;

    return {static_cast<float>(r) / 31.0f, static_cast<float>(g) / 63.0f, static_cast<float>(b) / 31.0f, 1.0f};
}

inline auto ColorPF5551ToRGBA8(uint16_t color) -> glm::fvec4 {
    const auto r = uint16_t{0b1111100000000000} & color;
    const auto g = uint16_t{0b0000011111000000} & color;
    const auto b = uint16_t{0b0000000000111110} & color;
    const auto a = uint16_t{0b0000000000000001} & color;

    return {
        static_cast<float>(r) / 31.0f, static_cast<float>(g) / 31.0f, static_cast<float>(b) / 31.0f,
        static_cast<float>(a)};
}

inline auto ColorPF4444ToRGBA8(uint16_t color) -> glm::fvec4 {
    const auto r = uint16_t{0xf000} & color;
    const auto g = uint16_t{0x0f00} & color;
    const auto b = uint16_t{0x00f0} & color;
    const auto a = uint16_t{0x000f} & color;

    return {
        static_cast<float>(r) / 15.0f, static_cast<float>(g) / 15.0f, static_cast<float>(b) / 15.0f,
        static_cast<float>(a) / 15.0f};
}

inline auto ColorPF8888ToRGBA8(uint32_t color) -> glm::fvec4 {
    const auto r = 0xff000000 & color;
    const auto g = 0x00ff0000 & color;
    const auto b = 0x0000ff00 & color;
    const auto a = 0x000000ff & color;

    return {
        static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f,
        static_cast<float>(a) / 255.0f};
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

    // sometimes the file will end with a half-chunk
    // we need to handle this kind of behavior here, because otherwise an exception
    // will be thrown, half chunks are smaller than full chunks!
    if ((SCEGMO_HALF_CHUNK & chunk.type) == 0) {
        chunk.child_offset = AssertRead<uint32_t>(reader, "invalid chunk");
        chunk.data_offset = AssertRead<uint32_t>(reader, "invalid chunk");
    }

    // for now, don't initialize those, they will be initialized later
    chunk.self_id = 0;
    chunk.next_id = std::nullopt;
    chunk.child_id = std::nullopt;

    return chunk;
}

auto ReadChunkFull(util::bytes::BinaryReader &parent_reader) -> GmoChunk {
    auto reader = parent_reader;
    auto chunk = ReadChunk(reader);

    const auto name_offset = GetChunkNameOffset(chunk);
    const auto name_length = GetChunkNameSize(chunk);

    if (name_offset.has_value()) {
        AssertSeek(reader, name_offset.value());
        const auto name_bytes = reader.ReadBuffer(name_length);

        if (!name_bytes.has_value()) {
            throw GmoParseError{"invalid chunk: name cannot be read"};
        }

        chunk.name = std::string{name_bytes->begin(), name_bytes->end()};
    }

    return chunk;
}

auto ReadMat4(util::bytes::BinaryReader &reader) -> glm::fmat4x4 {
    std::array<float, 16> values;
    for (uint32_t j = 0; j < 16; ++j) {
        values[j] = AssertRead<float>(reader);
    }

    return glm::make_mat4(values.data());
}

template <uint32_t D> auto AssertReadFVecN(util::bytes::BinaryReader &reader) -> glm::vec<D, float> {
    glm::vec<D, float> val;
    for (uint32_t i = 0; i < D; ++i) {
        val[i] = static_cast<float>(AssertRead<float>(reader));
    }

    return val;
};

template <typename T, uint32_t D> auto AssertReadFVecN(util::bytes::BinaryReader &reader) -> glm::vec<D, float> {
    constexpr float kRange = static_cast<float>(std::numeric_limits<T>::max());
    glm::vec<D, float> val;
    for (uint32_t i = 0; i < D; ++i) {
        val[i] = static_cast<float>(AssertRead<T>(reader)) / kRange;
    }

    return val;
};

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
            throw GmoParseError{fmt::format("cannot load bone from non-bone chunk with name {}", bone_chunk.name)};
        }

        const auto num_draw_parts = CountChildrenOfType(bone_chunk, SCEGMO_DRAW_PART);

        GmoBone bone;
        bone.name = bone_chunk.name;

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
                throw GmoParseError{fmt::format("SCEGMO_MORPH_WEIGHTS is unsupported in chunk {}", bone_chunk.name)};
            }

            case SCEGMO_MORPH_INDEX: {
                throw GmoParseError{fmt::format("SCEGMO_MORPH_INDEX is unsupported in chunk {}", bone.name)};
            }

            case SCEGMO_BLEND_BONES: {
                AssertSeek(reader, args_offset);
                const auto num_bones = AssertRead<uint32_t>(reader);

                if (num_bones > 8) {
                    throw GmoParseError{fmt::format(
                        "cannot have more than 8 blend bones, requested: {}, in chunk {}", num_bones, bone_chunk.name)};
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
                // TODO: for patapon, zsort is unk0 == 0, then unk1 is just the value
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                const auto unknown0 = AssertRead<uint32_t>(reader);
                const auto unknown1 = AssertRead<uint32_t>(reader);

                GMO_DEBUG_PRINT("type 226, unknown0 = {}, unknown1 = {}", unknown0, unknown1);
                break;
            }

            default:
                GMO_DEBUG_PRINT("skipping chunk type for bone {} with name {}", type, bone_chunk.name);
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

    auto LoadVertexArray(const GmoChunk &array_chunk) const -> GmoVertexArray {
        util::bytes::BinaryReader reader{buffer_};

        if (GetChunkType(array_chunk) != SCEGMO_ARRAYS) {
            throw GmoParseError{fmt::format("cannot load array from non-array chunk with name {}", array_chunk.name)};
        }

        GmoVertexArray array;
        array.name = array_chunk.name;

        AssertSeek(reader, GetChunkArgsOffset(array_chunk));

        const auto native_format = AssertRead<uint32_t>(reader);
        const auto native_num_verts = AssertRead<uint32_t>(reader);
        const auto native_num_morphs = AssertRead<uint32_t>(reader);
        [[maybe_unused]] const auto native_format2 = AssertRead<uint32_t>(reader);

        // if vertex morphs are to be supported this has to be handled
        if (native_num_morphs > 1) {
            throw GmoParseError{fmt::format("vertex morphs are not supported in chunk {}", array_chunk.name)};
        }

        // clang-format off
        const auto fmt_texture  = 0b0011 & (native_format >>  0);
        const auto fmt_color    = 0b0111 & (native_format >>  2);
        const auto fmt_normal   = 0b0011 & (native_format >>  5);
        const auto fmt_vertex   = 0b0011 & (native_format >>  7);
        const auto fmt_weight   = 0b0011 & (native_format >>  9);
        const auto fmt_index    = 0b0011 & (native_format >> 11);

        auto num_weights  = 0b1111 & (native_format >> 14);
        auto num_morphs   = 0b1111 & (native_format >> 18);
        // clang-format on

        if (fmt_texture != eSceGuFmtTextureNONE) {
            array.flags = array.flags | GmoVertexArrayFlags::eHasUvs;
        }

        if (fmt_color != eSceGuFmtColorNONE) {
            array.flags = array.flags | GmoVertexArrayFlags::eHasColor;
        }

        if (fmt_normal != eSceGuFmtNormalNONE) {
            array.flags = array.flags | GmoVertexArrayFlags::eHasNormals;
        }

        if (fmt_vertex != eSceGuFmtVertexNONE) {
            array.flags = array.flags | GmoVertexArrayFlags::eHasPositions;
        }

        if (fmt_weight != eSceGuFmtWeightNONE) {
            array.flags = array.flags | GmoVertexArrayFlags::eHasWeights;
            num_weights = num_weights + 1;
        } else {
            num_weights = 0;
        }

        array.num_weights = num_weights;

        GMO_DEBUG_PRINT(
            "load vertex array:\n"
            "\tfmt_texture:\t\t{}\n"
            "\tfmt_color:\t\t{}\n"
            "\tfmt_normal:\t\t{}\n"
            "\tfmt_vertex:\t\t{}\n"
            "\tfmt_weight:\t\t{}\n"
            "\tfmt_index:\t\t{}\n"
            "\tnum_weights:\t\t{}\n"
            "\tnum_morphs:\t\t{}",
            ToString(static_cast<SceGuFmtTexture>(fmt_texture)), ToString(static_cast<SceGuFmtColor>(fmt_color)),
            ToString(static_cast<SceGuFmtNormal>(fmt_normal)), ToString(static_cast<SceGuFmtVertex>(fmt_vertex)),
            ToString(static_cast<SceGuFmtWeight>(fmt_weight)), ToString(static_cast<SceGuFmtIndex>(fmt_index)),
            num_weights, num_morphs);

        using Reader = util::bytes::BinaryReader;
        const auto fnReadUv = [&]() -> glm::fvec2 (*)(Reader &) {
            switch (fmt_texture) {
            case eSceGuFmtTextureNONE:
                return []([[maybe_unused]] Reader &r) { return glm::fvec2{0.0f, 0.0f}; };
            case eSceGuFmtTextureUBYTE:
                return [](Reader &r) { return AssertReadFVecN<uint8_t, 2>(r); };
            case eSceGuFmtTextureUSHORT:
                return [](Reader &r) { return AssertReadFVecN<uint16_t, 2>(r); };
            case eSceGuFmtTextureFLOAT:
                return [](Reader &r) { return AssertReadFVecN<2>(r); };
            default:
                throw GmoParseError{
                    fmt::format("unsupported texture uv format {} in chunk {}", fmt_texture, array_chunk.name)};
            }
        }();

        const auto fnReadColor = [&]() -> glm::fvec4 (*)(Reader &) {
            switch (fmt_color) {
            case eSceGuFmtColorNONE:
                return []([[maybe_unused]] Reader &r) { return glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f}; };
            case eSceGuFmtColorPF5650:
                return [](Reader &r) {
                    const auto rgba = AssertRead<uint16_t>(r);
                    return ColorPF5650ToRGBA8(rgba);
                };
            case eSceGuFmtColorPF5551:
                return [](Reader &r) {
                    const auto rgba = AssertRead<uint16_t>(r);
                    return ColorPF5551ToRGBA8(rgba);
                };
            case eSceGuFmtColorPF4444:
                return [](Reader &r) {
                    const auto rgba = AssertRead<uint16_t>(r);
                    return ColorPF4444ToRGBA8(rgba);
                };
            case eSceGuFmtColorPF8888:
                return [](Reader &r) {
                    const auto rgba = AssertRead<uint16_t>(r);
                    return ColorPF8888ToRGBA8(rgba);
                };
            default:
                throw GmoParseError{
                    fmt::format("unsupported vertex color format {} in chunk {}", fmt_color, array_chunk.name)};
            }
        }();

        const auto fnReadNormal = [&]() -> glm::fvec3 (*)(Reader &) {
            switch (fmt_normal) {
            case eSceGuFmtNormalNONE:
                return []([[maybe_unused]] Reader &r) { return glm::fvec3{0.0f, 0.0f, 0.0f}; };
            case eSceGuFmtNormalBYTE:
                return [](Reader &r) { return AssertReadFVecN<int8_t, 3>(r); };
            case eSceGuFmtNormalSHORT:
                return [](Reader &r) { return AssertReadFVecN<int16_t, 3>(r); };
            case eSceGuFmtNormalFLOAT:
                return [](Reader &r) { return AssertReadFVecN<3>(r); };
            default:
                throw GmoParseError{
                    fmt::format("unsupported vertex normal format {} in chunk {}", fmt_normal, array_chunk.name)};
            }
        }();

        const auto fnReadPosition = [&]() -> glm::fvec3 (*)(Reader &) {
            switch (fmt_vertex) {
            case eSceGuFmtVertexNONE:
                return []([[maybe_unused]] Reader &r) { return glm::fvec3{0.0f, 0.0f, 0.0f}; };
            case eSceGuFmtVertexBYTE:
                return [](Reader &r) { return AssertReadFVecN<int8_t, 3>(r); };
            case eSceGuFmtVertexSHORT:
                return [](Reader &r) { return AssertReadFVecN<int16_t, 3>(r); };
            case eSceGuFmtVertexFLOAT:
                return [](Reader &r) { return AssertReadFVecN<3>(r); };
            default:
                throw GmoParseError{
                    fmt::format("unsupported vertex position format {} in chunk {}", fmt_vertex, array_chunk.name)};
            }
        }();

        const auto fnReadWeight = [&]() -> float (*)(Reader &) {
            switch (fmt_weight) {
            case eSceGuFmtWeightNONE:
                return []([[maybe_unused]] Reader &r) { return 0.0f; };
            case eSceGuFmtWeightUBYTE:
                return []([[maybe_unused]] Reader &r) { return static_cast<float>(AssertRead<uint8_t>(r)) / 255.0f; };
            case eSceGuFmtWeightUSHORT:
                return
                    []([[maybe_unused]] Reader &r) { return static_cast<float>(AssertRead<uint16_t>(r)) / 65535.0f; };
            case eSceGuFmtWeightFLOAT:
                return []([[maybe_unused]] Reader &r) { return AssertRead<float>(r); };
            default:
                throw GmoParseError{
                    fmt::format("unsupported vertex weight format {} in chunk {}", fmt_weight, array_chunk.name)};
            }
        }();

        array.vertices.reserve(native_num_verts);
        for (uint32_t i = 0; i < native_num_verts; ++i) {
            GmoVertex vertex;

            for (uint32_t w = 0; w < num_weights; ++w) {
                vertex.weights[w] = fnReadWeight(reader);
            }

            vertex.uv = fnReadUv(reader);
            vertex.color = fnReadColor(reader);
            vertex.normal = fnReadNormal(reader);
            vertex.position = fnReadPosition(reader);

            array.vertices.emplace_back(vertex);
        }

        return array;
    }

    auto LoadMesh(const GmoChunk &mesh_chunk) const -> GmoMesh {
        util::bytes::BinaryReader reader{buffer_};

        if (GetChunkType(mesh_chunk) != SCEGMO_MESH) {
            throw GmoParseError{fmt::format("cannot load mesh from non-mesh chunk with name {}", mesh_chunk.name)};
        }

        GmoMesh mesh;
        mesh.name = mesh_chunk.name;

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

            // these calls are unsupported by the psp
            case SCEGMO_SUBDIVISION: {
                throw GmoParseError{fmt::format("SCEGMO_SUBDIVISION is unsupported in chunk {}", mesh_chunk.name)};
            }

            case SCEGMO_KNOT_VECTOR_U: {
                throw GmoParseError{fmt::format("SCEGMO_KNOT_VECTOR_U is unsupported in chunk {}", mesh_chunk.name)};
            }

            case SCEGMO_KNOT_VECTOR_V: {
                throw GmoParseError{fmt::format("SCEGMO_KNOT_VECTOR_V is unsupported in chunk {}", mesh_chunk.name)};
            }

            case SCEGMO_DRAW_PARTICLE: {
                throw GmoParseError{fmt::format("SCEGMO_DRAW_PARTICLE is unsupported in chunk {}", mesh_chunk.name)};
            }

            // i would really love to implement splines
            // but i have no models to test them on
            // feel free to send me some if you are interested!
            case SCEGMO_DRAW_B_SPLINE: {
                throw GmoParseError{fmt::format("SCEGMO_DRAW_B_SPLINE is unsupported in chunk {}", mesh_chunk.name)};
            }

            case SCEGMO_DRAW_RECT_MESH: {
                throw GmoParseError{fmt::format("SCEGMO_DRAW_RECT_MESH is unsupported in chunk {}", mesh_chunk.name)};
            }

            case SCEGMO_DRAW_RECT_PATCH: {
                throw GmoParseError{fmt::format("SCEGMO_DRAW_RECT_PATCH is unsupported in chunk {}", mesh_chunk.name)};
            }

            case SCEGMO_DRAW_ARRAYS: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                GmoDrawArray draw;

                const auto native_arrays = AssertRead<uint32_t>(reader);
                const auto native_mode = AssertRead<uint32_t>(reader);
                const auto native_num_verts = AssertRead<uint32_t>(reader);
                const auto native_num_prims = AssertRead<uint32_t>(reader);

                draw.array_id = RefIndex(native_arrays);
                draw.num_vertices = native_num_verts;
                draw.num_primitives = native_num_prims;

                switch (native_mode & GmoPrimitiveFlags::SCEGMO_PRIM_TYPE_MASK) {
                case GmoPrimitiveFlags::SCEGMO_PRIM_POINTS:
                    draw.primitive = GmoDrawPrimitive::ePoints;
                    break;
                case GmoPrimitiveFlags::SCEGMO_PRIM_LINES:
                    draw.primitive = GmoDrawPrimitive::eLines;
                    break;
                case GmoPrimitiveFlags::SCEGMO_PRIM_LINE_STRIP:
                    draw.primitive = GmoDrawPrimitive::eLineStrip;
                    break;
                case GmoPrimitiveFlags::SCEGMO_PRIM_TRIANGLES:
                    draw.primitive = GmoDrawPrimitive::eTriangles;
                    break;
                case GmoPrimitiveFlags::SCEGMO_PRIM_TRIANGLE_STRIP:
                    draw.primitive = GmoDrawPrimitive::eTriangleStrip;
                    break;
                case GmoPrimitiveFlags::SCEGMO_PRIM_TRIANGLE_FAN:
                    draw.primitive = GmoDrawPrimitive::eTriangleFan;
                    break;
                case GmoPrimitiveFlags::SCEGMO_PRIM_RECTANGLES:
                    draw.primitive = GmoDrawPrimitive::eRectangles;
                    break;
                default:
                    throw GmoParseError{
                        fmt::format("unsupported primitive type {} in chunk {}", native_mode, mesh_chunk.name)};
                }

                // sequential means unindexed
                // instead the first index will be a uint16_t representing the vertex offset to draw from
                if (GmoPrimitiveFlags::SCEGMO_PRIM_SEQUENTIAL & native_mode) {
                    draw.indices.reserve(native_num_verts);

                    auto index = AssertRead<uint16_t>(reader);
                    for (uint32_t i = 0; i < native_num_verts; ++i) {
                        draw.indices.emplace_back(index++);
                    }
                } else {
                    draw.indices.reserve(native_num_verts);
                    for (uint32_t i = 0; i < native_num_verts; ++i) {
                        draw.indices.emplace_back(AssertRead<uint16_t>(reader));
                    }
                }

                mesh.draw_arrays.emplace_back(draw);
                break;
            }

            default:
                GMO_DEBUG_PRINT("skipping chunk type for mesh {} with name {}", type, mesh_chunk.name);
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
            throw GmoParseError{fmt::format("cannot load part from non-part chunk with name {}", part_chunk.name)};
        }

        GmoPart part;
        part.name = part_chunk.name;

        auto current_id = part_chunk.child_id;
        while (current_id.has_value()) {
            const auto &chunk = map_[current_id.value()];
            const auto type = GetChunkType(chunk);

            switch (type) {
            case SCEGMO_MESH:
                part.meshes.emplace_back(LoadMesh(chunk));
                break;

            case SCEGMO_ARRAYS:
                part.vertex_arrays.emplace_back(LoadVertexArray(chunk));
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
                GMO_DEBUG_PRINT("skipping chunk type for part {} in chunk {}", type, part_chunk.name);
                break;
            }

            current_id = chunk.next_id;
        }

        return part;
    }

    auto LoadMaterialLayer(const GmoChunk &layer_chunk) const -> GmoMaterialLayer {
        util::bytes::BinaryReader reader{buffer_};

        if (GetChunkType(layer_chunk) != SCEGMO_LAYER) {
            throw GmoParseError{fmt::format("cannot load layer from non-layer chunk with name {}", layer_chunk.name)};
        }

        GmoMaterialLayer layer;
        layer.name = layer_chunk.name;

        auto current_id = layer_chunk.child_id;
        while (current_id.has_value()) {
            const auto &chunk = map_[current_id.value()];
            const auto type = GetChunkType(chunk);

            switch (type) {
            case SCEGMO_SET_TEXTURE:
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                layer.texture_id = RefIndex(AssertRead<uint32_t>(reader));
                break;

            case SCEGMO_MAP_TYPE: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                const auto native_type = AssertRead<uint32_t>(reader);
                switch (native_type) {
                case SCEGMO_DIFFUSE:
                    layer.map_type = GmoMaterialLayerMapType::eDiffuse;
                    break;
                case SCEGMO_SPECULAR:
                    layer.map_type = GmoMaterialLayerMapType::eSpecular;
                    break;
                case SCEGMO_EMISSION:
                    layer.map_type = GmoMaterialLayerMapType::eEmission;
                    break;
                case SCEGMO_AMBIENT:
                    layer.map_type = GmoMaterialLayerMapType::eAmbient;
                    break;
                case SCEGMO_REFLECTION:
                    layer.map_type = GmoMaterialLayerMapType::eReflection;
                    break;
                case SCEGMO_REFRACTION:
                    layer.map_type = GmoMaterialLayerMapType::eRefraction;
                    break;
                default:
                    GMO_DEBUG_PRINT("invalid type {} for material layer with name {}", native_type, layer_chunk.name);
                    break;
                }

                break;
            }

            case SCEGMO_BLEND_FUNC: {
                constexpr std::array<GmoBlendOperator, 6> kBlendOps = {
                    GmoBlendOperator::eAdd, GmoBlendOperator::eSubtract, GmoBlendOperator::eReverseSubtract,
                    GmoBlendOperator::eMin, GmoBlendOperator::eMax,      GmoBlendOperator::eAbs};

                constexpr std::array<GmoBlendFunction, 10> kBlendFuncs = {
                    GmoBlendFunction::eFixValue, GmoBlendFunction::eFixValue,
                    GmoBlendFunction::eSrcColor, GmoBlendFunction::eOneMinusSrcColor,
                    GmoBlendFunction::eDstColor, GmoBlendFunction::eOneMinusDstColor,
                    GmoBlendFunction::eSrcAlpha, GmoBlendFunction::eOneMinusSrcAlpha,
                    GmoBlendFunction::eDstAlpha, GmoBlendFunction::eOneMinusDstAlpha,
                };

                AssertSeek(reader, GetChunkArgsOffset(chunk));
                const auto native_mode = AssertRead<uint32_t>(reader);
                const auto native_src = AssertRead<uint32_t>(reader);
                const auto native_dst = AssertRead<uint32_t>(reader);

                if (native_mode < 6) {
                    layer.blend_op = kBlendOps[native_mode];
                } else {
                    GMO_DEBUG_PRINT("invalid blend op {} for layer {}", native_mode, layer_chunk.name);
                }

                if (native_src < 10) {
                    layer.blend_func1 = kBlendFuncs[native_src];
                    layer.src_mask = native_src > 0 ? 0xffffffff : 0;
                } else {
                    GMO_DEBUG_PRINT("invalid blend src {} for layer {}", native_src, layer_chunk.name);
                }

                if (native_dst < 10) {
                    layer.blend_func1 = kBlendFuncs[native_dst];
                    layer.dst_mask = native_dst > 0 ? 0xffffffff : 0;
                } else {
                    GMO_DEBUG_PRINT("invalid blend dst {} for layer {}", native_dst, layer_chunk.name);
                }

                break;
            }

            case SCEGMO_TEX_CROP: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                const auto x = AssertRead<float>(reader);
                const auto y = AssertRead<float>(reader);
                const auto w = AssertRead<float>(reader);
                const auto h = AssertRead<float>(reader);

                layer.texture_crop.min = {x, y};
                layer.texture_crop.max = layer.texture_crop.min + glm::fvec2{w, h};

                break;
            }

            default:
                GMO_DEBUG_PRINT("skipping chunk type for layer {} with name {}", type, layer_chunk.name);
                break;
            }

            current_id = chunk.next_id;
        }

        return layer;
    }

    auto LoadMaterial(const GmoChunk &material_chunk) const -> GmoMaterial {
        util::bytes::BinaryReader reader{buffer_};

        if (GetChunkType(material_chunk) != SCEGMO_MATERIAL) {
            throw GmoParseError{
                fmt::format("cannot load material from non-material chunk with name {}", material_chunk.name)};
        }

        GmoMaterial material;
        material.name = material_chunk.name;

        auto current_id = material_chunk.child_id;
        while (current_id.has_value()) {
            const auto &chunk = map_[current_id.value()];
            const auto type = GetChunkType(chunk);

            switch (type) {
            case SCEGMO_LAYER: {
                auto layer = LoadMaterialLayer(chunk);
                switch (layer.map_type) {
                case GmoMaterialLayerMapType::eDiffuse:
                    material.flags = material.flags | GmoMaterialFlags::eHasDiffuse;
                    break;
                case GmoMaterialLayerMapType::eAmbient:
                    material.flags = material.flags | GmoMaterialFlags::eHasAmbient;
                    break;
                case GmoMaterialLayerMapType::eEmission:
                    material.flags = material.flags | GmoMaterialFlags::eHasEmission;
                    break;
                case GmoMaterialLayerMapType::eReflection:
                    material.flags = material.flags | GmoMaterialFlags::eHasReflection;
                    break;
                case GmoMaterialLayerMapType::eRefraction:
                    material.flags = material.flags | GmoMaterialFlags::eHasRefraction;
                    break;
                case GmoMaterialLayerMapType::eSpecular:
                    material.flags = material.flags | GmoMaterialFlags::eHasSpecular;
                    break;
                default:
                    break;
                }

                material.layers.emplace_back(std::move(layer));
                break;
            }

            case SCEGMO_RENDER_STATE: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                const auto render_state = AssertRead<uint32_t>(reader);

                switch (render_state) {
                case SCEGMO_STATE_LIGHTING:
                    material.flags = material.flags | GmoMaterialFlags::eEnableLighting;
                    break;

                case SCEGMO_STATE_FOG:
                    material.flags = material.flags | GmoMaterialFlags::eEnableFog;
                    break;

                case SCEGMO_STATE_CULL_FACE:
                    material.flags = material.flags | GmoMaterialFlags::eEnableCullFace;
                    break;

                case SCEGMO_STATE_DEPTH_TEST:
                    material.flags = material.flags | GmoMaterialFlags::eEnableDepthTest;
                    break;

                case SCEGMO_STATE_DEPTH_MASK:
                    material.flags = material.flags | GmoMaterialFlags::eEnableDepthMask;
                    break;

                case SCEGMO_STATE_ALPHA_TEST:
                    material.flags = material.flags | GmoMaterialFlags::eEnableAlphaTest;
                    break;

                case SCEGMO_STATE_ALPHA_MASK:
                    material.flags = material.flags | GmoMaterialFlags::eEnableAlphaMask;
                    break;

                default:
                    break;
                }
            }

            case SCEGMO_DIFFUSE: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                material.colors[eGmoMaterialColorDiffuse].r = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorDiffuse].g = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorDiffuse].b = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorDiffuse].a = AssertRead<float>(reader);
                break;
            }

            case SCEGMO_SPECULAR: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                material.colors[eGmoMaterialColorSpecular].r = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorSpecular].g = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorSpecular].b = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorSpecular].a = AssertRead<float>(reader);
                material.shininess = AssertRead<float>(reader);
                break;
            }

            case SCEGMO_EMISSION: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                material.colors[eGmoMaterialColorEmission].r = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorEmission].g = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorEmission].b = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorEmission].a = AssertRead<float>(reader);
                break;
            }

            case SCEGMO_AMBIENT: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                material.colors[eGmoMaterialColorAmbient].r = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorAmbient].g = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorAmbient].b = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorAmbient].a = AssertRead<float>(reader);
                break;
            }

            case SCEGMO_REFLECTION: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                material.colors[eGmoMaterialColorReflection].r = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorReflection].g = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorReflection].b = AssertRead<float>(reader);
                material.colors[eGmoMaterialColorReflection].a = AssertRead<float>(reader);
                break;
            }

            case SCEGMO_REFRACTION: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                material.refraction = AssertRead<float>(reader);
                break;
            }

            default:
                GMO_DEBUG_PRINT("skipping chunk type for material {} with name {}", type, material_chunk.name);
                break;
            }

            current_id = chunk.next_id;
        }

        return material;
    }

    auto LoadTexture(const GmoChunk &texture_chunk) const -> GmoTexture {
        util::bytes::BinaryReader reader{buffer_};

        if (GetChunkType(texture_chunk) != SCEGMO_TEXTURE) {
            throw GmoParseError{
                fmt::format("cannot load texture from non-texture chunk with name {}", texture_chunk.name)};
        }

        GmoTexture texture;
        texture.name = texture_chunk.name;

        auto current_id = texture_chunk.child_id;
        while (current_id.has_value()) {
            const auto &chunk = map_[current_id.value()];
            const auto type = GetChunkType(chunk);

            switch (type) {
            case SCEGMO_FILE_NAME: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                std::array<char, 256> name_buffer;

                std::fill(name_buffer.begin(), name_buffer.end(), 0);
                uint8_t ch = AssertRead<uint8_t>(reader);
                size_t idx = 0;

                while (ch && idx < 255) {
                    name_buffer[idx++] = ch;
                    ch = AssertRead<uint8_t>(reader);
                }

                texture.filename = std::string{name_buffer.data()};
                break;
            }

            case SCEGMO_FILE_IMAGE: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                const auto size = AssertRead<uint32_t>(reader);
                const auto read_buffer = reader.ReadBuffer(size);

                if (!read_buffer.has_value()) {
                    throw GmoParseError{fmt::format(
                        "cannot read {} bytes for texture data in texture chunk {}", size, texture_chunk.name)};
                }

                GMO_DEBUG_PRINT(
                    "gmo texture chunk {} has binary texture in +{}, sized {}", texture_chunk.name,
                    GetChunkArgsOffset(chunk), size);
                texture.data.assign(read_buffer->begin(), read_buffer->end());
                break;
            }

            default:
                GMO_DEBUG_PRINT("skipping chunk type for texture {} with name {}", type, texture_chunk.name);
                break;
            }

            current_id = chunk.next_id;
        }

        return texture;
    }

    auto LoadFCurve(const GmoChunk &fcurve_chunk) const -> GmoFCurve {
        util::bytes::BinaryReader reader{buffer_};

        if (GetChunkType(fcurve_chunk) != SCEGMO_FCURVE) {
            throw GmoParseError{
                fmt::format("cannot load fcurve from non-fcurve chunk with name {}", fcurve_chunk.name)};
        }

        GmoFCurve fcurve;
        fcurve.name = fcurve_chunk.name;

        AssertSeek(reader, GetChunkArgsOffset(fcurve_chunk));
        const auto native_format = AssertRead<uint32_t>(reader);
        const auto native_num_dims = AssertRead<uint32_t>(reader);
        const auto native_num_keys = AssertRead<uint32_t>(reader);

        (void)AssertRead<uint32_t>(reader); // unused

        constexpr std::array<uint32_t, 5> kElementsPerInterpType = {1, 1, 3, 5, 1};
        const auto interpolation = native_format & SCEGMO_FCURVE_INTERP_MASK;

        switch (interpolation) {
        case SCEGMO_FCURVE_CONSTANT:
            fcurve.interpolation = GmoFCurveInterpolation::eConstant;
            break;

        case SCEGMO_FCURVE_LINEAR:
            fcurve.interpolation = GmoFCurveInterpolation::eLinear;
            break;

        case SCEGMO_FCURVE_HERMITE:
            fcurve.interpolation = GmoFCurveInterpolation::eHermite;
            break;

        case SCEGMO_FCURVE_CUBIC:
            fcurve.interpolation = GmoFCurveInterpolation::eCubic;
            break;

        case SCEGMO_FCURVE_SPHERICAL:
            fcurve.interpolation = GmoFCurveInterpolation::eSpherical;
            break;

        default:
            GMO_DEBUG_PRINT("invalid fcurve interpolation type {} in fcurve {}", interpolation, fcurve_chunk.name);
            fcurve.interpolation = GmoFCurveInterpolation::eConstant;
            break;
        }

        fcurve.dimensions = native_num_dims;
        fcurve.num_keyframes = native_num_keys;

        const auto num_elements = kElementsPerInterpType[static_cast<uint32_t>(fcurve.interpolation)];
        const auto stride = num_elements * native_num_dims + 1;

        const auto total_size = stride * native_num_keys;
        const auto total_size_bytes = sizeof(float) * total_size;

        const auto buffer = reader.ReadBuffer(total_size_bytes);
        if (!buffer.has_value()) {
            throw GmoParseError{
                fmt::format("invalid fcurve data block in fcurve {}, not enough data!", fcurve_chunk.name)};
        }

        // TODO: endianness?
        fcurve.raw_data.resize(total_size);
        std::memcpy(fcurve.raw_data.data(), buffer->data(), total_size_bytes);

        return fcurve;
    }

    auto LoadMotion(const GmoChunk &motion_chunk) const -> GmoMotion {
        util::bytes::BinaryReader reader{buffer_};

        if (GetChunkType(motion_chunk) != SCEGMO_MOTION) {
            throw GmoParseError{
                fmt::format("cannot load motion from non-motion chunk with name {}", motion_chunk.name)};
        }

        GmoMotion motion;
        motion.name = motion_chunk.name;

        auto current_id = motion_chunk.child_id;
        while (current_id.has_value()) {
            const auto &chunk = map_[current_id.value()];
            const auto type = GetChunkType(chunk);

            switch (type) {
            case SCEGMO_FRAME_RATE:
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                motion.framerate = AssertRead<float>(reader);
                break;

            case SCEGMO_FRAME_LOOP:
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                motion.frame_loop_start = AssertRead<float>(reader);
                motion.frame_loop_end = AssertRead<float>(reader);
                break;

            case SCEGMO_FCURVE:
                motion.fcurves.emplace_back(LoadFCurve(chunk));
                break;

            case SCEGMO_ANIMATE: {
                AssertSeek(reader, GetChunkArgsOffset(chunk));
                const auto native_block = AssertRead<uint32_t>(reader);
                const auto native_cmd = AssertRead<uint32_t>(reader);
                const auto native_index = AssertRead<uint32_t>(reader);
                const auto native_fcurve = AssertRead<uint32_t>(reader);

                GmoAnimation anim;

                anim.fcurve_id = RefIndex(native_fcurve);
                anim.index = native_index;
                anim.target_id = RefIndex(native_block);

                const auto ref_type = RefType(native_block);
                switch (ref_type) {
                case SCEGMO_BONE:
                    anim.target = GmoAnimationTarget::eBone;
                    break;

                case SCEGMO_MATERIAL:
                    anim.target = GmoAnimationTarget::eMaterial;
                    break;

                default:
                    GMO_DEBUG_PRINT("invalid animate target type {} for motion {}", ref_type, motion_chunk.name);
                    break;
                }

                switch (native_cmd) {
                case SCEGMO_TRANSLATE:
                    anim.property = eAnimBoneTranslate;
                    break;

                case SCEGMO_ROTATE_Q:
                    anim.property = eAnimBoneRotateQ;
                    break;

                case SCEGMO_ROTATE_ZYX:
                    anim.property = eAnimBoneRotateZYX;
                    break;

                case SCEGMO_ROTATE_YXZ:
                    anim.property = eAnimBoneRotateYXZ;
                    break;

                case SCEGMO_SCALE:
                    anim.property = eAnimBoneScale1;
                    break;

                case SCEGMO_SCALE_2:
                    anim.property = eAnimBoneScale2;
                    break;

                case SCEGMO_SCALE_3:
                    anim.property = eAnimBoneScale3;
                    break;

                case SCEGMO_MULT_MATRIX:
                    anim.property = eAnimBoneMultMatrix;
                    break;

                case SCEGMO_MORPH_WEIGHTS:
                    anim.property = eAnimBoneMorphWeights;
                    break;

                case SCEGMO_MORPH_INDEX:
                    anim.property = eAnimBoneMorphIndex;
                    break;

                case SCEGMO_VISIBILITY:
                    anim.property = eAnimBoneVisibility;
                    break;

                case SCEGMO_DIFFUSE:
                    anim.property = eAnimMaterialDiffuse;
                    break;

                case SCEGMO_SPECULAR:
                    anim.property = eAnimMaterialSpecular;
                    break;

                case SCEGMO_EMISSION:
                    anim.property = eAnimMaterialEmission;
                    break;

                case SCEGMO_AMBIENT:
                    anim.property = eAnimMaterialAmbient;
                    break;

                case SCEGMO_REFLECTION:
                    anim.property = eAnimMaterialReflection;
                    break;

                case SCEGMO_REFRACTION:
                    anim.property = eAnimMaterialRefraction;
                    break;

                case SCEGMO_BUMP:
                    anim.property = eAnimMaterialBump;
                    break;

                case SCEGMO_TEX_CROP:
                    anim.property = eAnimMaterialTextureCrop;
                    break;

                default:
                    anim.property = static_cast<GmoAnimationProperty>(native_cmd);
                    GMO_DEBUG_PRINT("using custom property for animation {} for motion {}", native_cmd, motion.name);
                    break;
                }

                motion.animations.emplace_back(anim);
                break;
            }

            default:
                GMO_DEBUG_PRINT("skipping chunk type for motion {} with name {}", type, motion_chunk.name);
                break;
            }

            current_id = chunk.next_id;
        }

        return motion;
    }

    auto LoadModel(const GmoChunk &model_chunk) const -> GmoModel {
        util::bytes::BinaryReader reader{buffer_};

        if (GetChunkType(model_chunk) != SCEGMO_MODEL) {
            throw GmoParseError{fmt::format("cannot load model from non-model chunk with name {}", model_chunk.name)};
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
        model.name = model_chunk.name;

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
                model.materials.emplace_back(LoadMaterial(chunk));
                break;

            case SCEGMO_TEXTURE:
                model.textures.emplace_back(LoadTexture(chunk));
                break;

            case SCEGMO_MOTION:
                model.motions.emplace_back(LoadMotion(chunk));
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
                GMO_DEBUG_PRINT("skipping chunk type for model {} with name {}", type, model_chunk.name);
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

            const auto current_chunk_id = InsertChunk(ReadChunkFull(reader));
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

            const auto current_chunk_id = InsertChunk(ReadChunkFull(reader));
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

auto CheckHeader(std::span<const uint8_t> buffer) -> bool {
    util::bytes::BinaryReader reader{buffer};
    try {
        (void)GetGmoHeader(reader);
    } catch (const std::exception &) {
        return false;
    }

    return true;
}

} // namespace gmo
