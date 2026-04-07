#include <stdexcept>
#include <fmt/format.h>

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

    if (header.signature != SCEGMO_FORMAT_SIGNATURE) {
        throw GmoParseError{"incorrect file signature"};
    }

    if (header.version != SCEGMO_FORMAT_VERSION) {
        throw GmoParseError{"incorrect file signature"};
    }

    if (header.style != SCEGMO_FORMAT_STYLE_PSP) {
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
    auto LoadModel(const GmoChunk &chunk) const -> GmoModel {
        if (GetChunkType(chunk) != SCEGMO_MODEL) {
            throw GmoParseError{"cannot load model from non-model chunk"};
        }

        const auto num_bone_chunks = CountChildrenOfType(chunk, SCEGMO_BONE);
        const auto num_part_chunks = CountChildrenOfType(chunk, SCEGMO_PART);
        const auto num_material_chunks = CountChildrenOfType(chunk, SCEGMO_MATERIAL);
        const auto num_texture_chunks = CountChildrenOfType(chunk, SCEGMO_TEXTURE);
        const auto num_motion_chunks = CountChildrenOfType(chunk, SCEGMO_MOTION);

        GMO_DEBUG_PRINT(
            "validated gmo model:\n\tbones:\t\t{}\n\tparts:\t\t{}\n\tmaterials:\t{}\n\ttextures:\t{}\n\tmotions:\t{}",
            num_bone_chunks, num_part_chunks, num_material_chunks, num_texture_chunks, num_motion_chunks);

        return {};
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
