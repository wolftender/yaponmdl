#include <fmt/format.h>

#include "formats/gxp.hpp"
#include "common/binaryreader.hpp"

namespace gxp {

constexpr uint32_t kGxpMagicNumber = 0x2e475858;
constexpr uint32_t kGxpVersion = 0x302e3130;
constexpr uint32_t kGxpStyleModel = 0x2e474d4f;
constexpr uint32_t kGxpStyleTexture = 0x2e47494d;

using GxpParseError = std::runtime_error;

constexpr auto kGxpByteOrder = util::bytes::BinaryReader::ByteOrder::eLittleEndian;

template <typename T>
auto AssertRead(util::bytes::BinaryReader &reader, std::optional<std::string_view> message = std::nullopt) -> T {
    const auto value = reader.Read<kGxpByteOrder, T>();
    if (!value.has_value()) {
        if (message.has_value()) {
            throw GxpParseError{std::string{message.value()}};
        } else {
            throw GxpParseError{fmt::format("invalid byte read at position {}", reader.Position())};
        }
    }

    return value.value();
}

auto AssertSeek(util::bytes::BinaryReader &reader, uint64_t position) -> void {
    const auto res = reader.Seek(position);
    if (util::bytes::BinaryReader::Result::eSuccess != res) {
        throw GxpParseError{fmt::format("invalid gxt offset calculated {}", position)};
    }
}

template <typename T> auto AssertReadAt(util::bytes::BinaryReader &reader, uint64_t offset) -> T {
    AssertSeek(reader, offset);
    return AssertRead<T>(reader);
}

auto AssertReadStringAt(util::bytes::BinaryReader &reader, uint64_t offset) -> std::span<const uint8_t> {
    AssertSeek(reader, offset);

    size_t length = 0;
    uint8_t chr = 0;

    do {
        const auto res = reader.Read<kGxpByteOrder, uint8_t>();
        if (!res.has_value()) {
            break;
        }

        chr = res.value();
        ++length;
    } while (chr);

    AssertSeek(reader, offset);
    const auto buffer = reader.ReadBuffer(length - 1);

    if (!buffer.has_value()) {
        throw GxpParseError{fmt::format("cannot read string at {}", offset)};
    }

    return buffer.value();
}

class GxpConsoleLogger final : public GxpLogger {
public:
    auto log(std::string_view log_message) const -> void override {
        fmt::println("[libgxx console log] {}", log_message);
    }
};

auto LoadPixelSection(const util::bytes::BinaryReader &parent_reader, uint32_t offset, const GxpLogger &logger)
    -> SceGxpBuffer {
    util::bytes::BinaryReader reader{parent_reader};
    AssertSeek(reader, offset);

    SceGxpBuffer buffer;
    buffer.offset = offset;
    buffer.type = AssertRead<uint32_t>(reader);
    buffer.size = AssertRead<uint32_t>(reader);
    buffer.capacity = AssertRead<uint32_t>(reader);
    buffer.owner = AssertRead<uint32_t>(reader);

    if (buffer.type != 2) {
        logger.log(fmt::format("warning: pixel section has type: {} != 2", buffer.type));
    }

    return buffer;
}

auto LoadVertexSection(const util::bytes::BinaryReader &parent_reader, uint32_t offset, const GxpLogger &logger)
    -> SceGxpBuffer {
    util::bytes::BinaryReader reader{parent_reader};
    AssertSeek(reader, offset);

    SceGxpBuffer buffer;
    buffer.offset = offset;
    buffer.type = AssertRead<uint32_t>(reader);
    buffer.size = AssertRead<uint32_t>(reader);
    buffer.capacity = AssertRead<uint32_t>(reader);
    buffer.owner = AssertRead<uint32_t>(reader);

    if (buffer.type != 1) {
        logger.log(fmt::format("warning: vertex section has type: {} != 1", buffer.type));
    }

    return buffer;
}

auto LoadPacketSection(const util::bytes::BinaryReader &parent_reader, uint32_t offset, const GxpLogger &logger)
    -> SceGxpBuffer {
    util::bytes::BinaryReader reader{parent_reader};
    AssertSeek(reader, offset);

    SceGxpBuffer buffer;
    buffer.offset = offset;
    buffer.type = AssertRead<uint32_t>(reader);
    buffer.size = AssertRead<uint32_t>(reader);
    buffer.capacity = AssertRead<uint32_t>(reader);
    buffer.owner = AssertRead<uint32_t>(reader);

    if (buffer.type != 0) {
        logger.log(fmt::format("warning: packet section has type: {} != 0", buffer.type));
    }

    return buffer;
}

auto LoadNodeSection(
    const util::bytes::BinaryReader &parent_reader, uint32_t offset, [[maybe_unused]] const GxpLogger &logger)
    -> SceGxpNode {
    util::bytes::BinaryReader reader{parent_reader};
    AssertSeek(reader, offset);

    SceGxpNode node;
    node.offset = offset;
    node.matrix_offs = AssertRead<uint32_t>(reader);
    node.name_offs = AssertRead<uint32_t>(reader);

    const auto name_buffer = AssertReadStringAt(reader, node.name_offs);
    node.name = std::string{name_buffer.begin(), name_buffer.end()};

    return node;
}

auto LoadMotionSection(
    const util::bytes::BinaryReader &parent_reader, uint32_t offset, [[maybe_unused]] const GxpLogger &logger)
    -> SceGxpMotion {
    util::bytes::BinaryReader reader{parent_reader};
    AssertSeek(reader, offset);

    SceGxpMotion motion;
    motion.offset = offset;
    motion.frame_offs = AssertRead<uint32_t>(reader);
    motion.num_frames = AssertRead<uint32_t>(reader);
    motion.framerate = AssertRead<float>(reader);
    motion.reserved = AssertRead<uint32_t>(reader);

    if (motion.num_frames > 0) {
        for (uint32_t i = 0; i < motion.num_frames; ++i) {
            const uint32_t frame_offset = motion.frame_offs + (0x04 * i);

            SceGxpMotionFrame frame;
            frame.offset = frame_offset;
            frame.ge_buffer_offs = AssertReadAt<uint32_t>(reader, frame_offset);
            frame.ge_buffer_size = AssertReadAt<uint32_t>(reader, frame.ge_buffer_offs - 0x38); // upper bound

            motion.frames.emplace_back(frame);
        }
    }

    return motion;
}

auto LoadObjectSection(const util::bytes::BinaryReader &parent_reader, uint32_t offset, const GxpLogger &logger)
    -> SceGxpObject {
    util::bytes::BinaryReader reader{parent_reader};
    AssertSeek(reader, offset);

    SceGxpObject object;
    object.offset = offset;

    // clang-format off
    object.type             = AssertRead<uint32_t>(reader);
    object.name_offs        = AssertRead<uint32_t>(reader);
    object.children_offs    = AssertRead<uint32_t>(reader);
    object.num_children     = AssertRead<uint32_t>(reader);
    object.motion_offs      = AssertRead<uint32_t>(reader);
    object.num_motions      = AssertRead<uint32_t>(reader);
    object.node_offs        = AssertRead<uint32_t>(reader);
    object.num_nodes        = AssertRead<uint32_t>(reader);
    // clang-format on

    const auto name_buffer = AssertReadStringAt(reader, object.name_offs);
    object.name = std::string{name_buffer.begin(), name_buffer.end()};

    // read object child items
    if (object.num_nodes > 0) {
        for (uint32_t i = 0; i < object.num_nodes; ++i) {
            const uint32_t node_offset = object.node_offs + (0x08 * i);
            object.nodes.emplace_back(LoadNodeSection(reader, node_offset, logger));
        }
    }

    if (object.num_motions > 0) {
        for (uint32_t i = 0; i < object.num_motions; ++i) {
            const uint32_t motion_offset = object.motion_offs + (0x10 * i);
            object.motions.emplace_back(LoadMotionSection(reader, motion_offset, logger));
        }
    }

    if (object.num_children > 0) {
        AssertSeek(reader, object.children_offs);
        for (uint32_t i = 0; i < object.num_children; ++i) {
            const auto child_offs = AssertRead<uint32_t>(reader);
            object.children.emplace_back(LoadObjectSection(reader, child_offs, logger));
        }
    }

    return object;
}

auto LoadFromMemory(const std::span<const uint8_t> buffer, const GxpLogger *logger) -> SceGxpFile {
    GxpConsoleLogger console_logger;
    if (!logger) {
        logger = &console_logger;
    }

    util::bytes::BinaryReader reader{buffer};

    SceGxpFile file;

    // clang-format off
    file.magic          = AssertRead<uint32_t>(reader);
    file.version        = AssertRead<uint32_t>(reader);
    file.format         = AssertRead<uint32_t>(reader);
    file.option         = AssertRead<uint32_t>(reader);
    file.root_offs      = AssertRead<uint32_t>(reader);
    file.buffer_ptr     = AssertRead<uint32_t>(reader);
    file.reserved0x18   = AssertRead<uint32_t>(reader);
    file.reserved0x1c   = AssertRead<uint32_t>(reader);
    file.object_offs    = AssertRead<uint32_t>(reader);
    file.object_size    = AssertRead<uint32_t>(reader);
    file.packet_offs    = AssertRead<uint32_t>(reader);
    file.packet_size    = AssertRead<uint32_t>(reader);
    file.vertex_offs    = AssertRead<uint32_t>(reader);
    file.vertex_size    = AssertRead<uint32_t>(reader);
    file.pixel_offs     = AssertRead<uint32_t>(reader);
    file.pixel_size     = AssertRead<uint32_t>(reader);
    // clang-format on

    // validate the header
    if (file.magic != kGxpMagicNumber || file.version != kGxpVersion ||
        (file.format != kGxpStyleModel && file.format != kGxpStyleTexture)) {
        throw GxpParseError{"invalid gxp file format"};
    }

    if (file.root_offs != file.object_offs) {
        logger->log(
            fmt::format(
                "warning: root offset is different from object offset: {} != {}", file.root_offs, file.object_offs));
    }

    if (file.pixel_size != 0) {
        file.root_pixel = LoadPixelSection(reader, file.pixel_offs, *logger);
    }

    if (file.vertex_size != 0) {
        file.root_vertex = LoadPacketSection(reader, file.vertex_offs, *logger);
    }

    if (file.packet_size != 0) {
        file.root_packet = LoadPacketSection(reader, file.packet_offs, *logger);
    }

    if (file.object_size == 0) {
        throw GxpParseError{"invalid gxp file format, no object section"};
    }

    file.root_object = LoadObjectSection(reader, file.object_offs, *logger);
    return file;
}

} // namespace gxp
