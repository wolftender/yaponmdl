#include <stdexcept>

#include <fmt/format.h>

#include "formats/pssg.hpp"
#include "common/binaryreader.hpp"

namespace pssg {

constexpr uint32_t kPssgMagicNumber = 0x50535347; // "PSSG"

using PssgParseError = std::runtime_error;

constexpr auto kPssgByteOrder = util::bytes::BinaryReader::ByteOrder::eBigEndian;

template <typename T>
auto AssertRead(util::bytes::BinaryReader &reader, std::optional<std::string_view> message = std::nullopt) -> T {
    const auto value = reader.Read<kPssgByteOrder, T>();
    if (!value.has_value()) {
        if (message.has_value()) {
            throw PssgParseError{std::string{message.value()}};
        } else {
            throw PssgParseError{fmt::format("invalid byte read at position {}", reader.Position())};
        }
    }

    return value.value();
}

auto AssertSeek(util::bytes::BinaryReader &reader, uint64_t position) -> void {
    const auto res = reader.Seek(position);
    if (util::bytes::BinaryReader::Result::eSuccess != res) {
        throw PssgParseError{fmt::format("invalid gxt offset calculated {}", position)};
    }
}

template <typename T> auto AssertReadAt(util::bytes::BinaryReader &reader, uint64_t offset) -> T {
    AssertSeek(reader, offset);
    return AssertRead<T>(reader);
}

class PssgConsoleLogger final : public PssgLogger {
public:
    auto log(std::string_view log_message) const -> void override {
        fmt::println("[libpssg console log] {}", log_message);
    }
};

auto ReadString(util::bytes::BinaryReader &reader) -> std::string_view {
    const auto pssg_str_len = AssertRead<uint32_t>(reader);
    const auto pssg_str_buf = reader.ReadBuffer(pssg_str_len);

    if (!pssg_str_buf.has_value()) {
        throw PssgParseError{fmt::format("failed to parse pssg sized string of length {}", pssg_str_len)};
    }

    return std::string_view{reinterpret_cast<const char *>(pssg_str_buf->data()), pssg_str_buf->size()};
}

constexpr auto FindSchemaElementType(std::string_view element_name) -> ScePssgElementType {
    const auto iter =
        std::find_if(kKnownElementTypes.begin(), kKnownElementTypes.end(), [&](const auto &known) -> bool {
        return element_name == known.name;
    });

    if (kKnownElementTypes.end() != iter) {
        return iter->type;
    }

    return ScePssgElementType::eUnknown;
}

constexpr auto FindSchemaAttributeType(std::string_view element_name, std::string_view attrib_name)
    -> ScePssgAttributeType {
    const auto iter = std::find_if(kKnownAttribTypes.begin(), kKnownAttribTypes.end(), [&](const auto &known) -> bool {
        return element_name == known.element && attrib_name == known.name;
    });

    if (kKnownAttribTypes.end() != iter) {
        return iter->type;
    }

    return ScePssgAttributeType::eUnknown;
}

auto ReadSchema(util::bytes::BinaryReader &reader) -> ScePssgSchema {
    const auto pssg_num_attribs = AssertRead<uint32_t>(reader);
    const auto pssg_num_elements = AssertRead<uint32_t>(reader);

    ScePssgSchema pssg_schema;
    pssg_schema.attributes.resize(pssg_num_attribs);
    pssg_schema.elements.resize(pssg_num_elements);

    for (uint32_t i = 0; i < pssg_num_elements; ++i) {
        const auto pssg_element_id = AssertRead<uint32_t>(reader);
        const auto pssg_element_name = ReadString(reader);

        if (pssg_element_id > pssg_num_elements) {
            throw PssgParseError{fmt::format("invalid pssg element id {}", pssg_element_id)};
        }

        auto &pssg_element = pssg_schema.elements[pssg_element_id - 1];

        pssg_element.id = pssg_element_id;
        pssg_element.name = pssg_element_name;
        pssg_element.type = FindSchemaElementType(pssg_element_name);

        const auto pssg_num_subattribs = AssertRead<uint32_t>(reader);
        for (uint32_t j = 0; j < pssg_num_subattribs; ++j) {
            const auto pssg_attrib_id = AssertRead<uint32_t>(reader);
            const auto pssg_attrib_name = ReadString(reader);

            if (pssg_attrib_id > pssg_num_attribs) {
                throw PssgParseError{fmt::format("invalid pssg attribute id {}", pssg_element_id)};
            }

            auto &pssg_attrib = pssg_schema.attributes[pssg_attrib_id - 1];

            pssg_attrib.id = pssg_attrib_id;
            pssg_attrib.name = pssg_attrib_name;
            pssg_attrib.element_id = pssg_element_id;
            pssg_attrib.type = FindSchemaAttributeType(pssg_element_name, pssg_attrib_name);
        }
    }

    return pssg_schema;
}

auto CheckChildren(const std::span<const uint8_t> buffer, const ScePssgSchema &pssg_schema) -> bool {
    util::bytes::BinaryReader reader{buffer};
    for (; reader.Remaining() > 0;) {
        const auto pssg_element_id = reader.Read<kPssgByteOrder, uint32_t>();

        // not a valid element id
        if (!pssg_element_id.has_value() || pssg_element_id > pssg_schema.elements.size()) {
            return false;
        }

        // not a valid element size
        const auto pssg_element_size = reader.Read<kPssgByteOrder, uint32_t>();
        if (!pssg_element_size.has_value() || reader.Remaining() < pssg_element_size.value()) {
            return false;
        }

        reader.Seek(reader.Position() + pssg_element_size.value());
    }

    return true;
}

auto ReadElement(
    util::bytes::BinaryReader &reader, const ScePssgSchema &pssg_schema, ScePssgFile &pssg_file,
    std::optional<uint32_t> parent_idx) -> uint32_t {
    const auto pssg_element_id = AssertRead<uint32_t>(reader);
    const auto pssg_element_size = AssertRead<uint32_t>(reader);
    const auto pssg_element_end_offs = reader.Position() + pssg_element_size;

    const auto pssg_element_attr_size = AssertRead<uint32_t>(reader);

    if (pssg_element_id > pssg_schema.elements.size()) {
        throw PssgParseError{fmt::format("pssg element references invalid schema element: {}", pssg_element_id)};
    }

    const auto pssg_attrib_buffer = reader.ReadBuffer(pssg_element_attr_size);
    if (!pssg_attrib_buffer.has_value()) {
        throw PssgParseError{"pssg element has invalid attribute buffer size"};
    }

    pssg_file.elements.emplace_back(
        ScePssgElement{
            .id = pssg_element_id,
            .parent = parent_idx,
        });

    const auto pssg_element_idx = static_cast<uint32_t>(pssg_file.elements.size() - 1);

    util::bytes::BinaryReader attrib_reader{pssg_attrib_buffer.value()};
    for (; attrib_reader.Remaining() > 0;) {
        const auto pssg_attrib_id = AssertRead<uint32_t>(attrib_reader);
        const auto pssg_attrib_size = AssertRead<uint32_t>(attrib_reader);
        const auto pssg_attrib_value = attrib_reader.ReadBuffer(pssg_attrib_size);

        if (pssg_attrib_id > pssg_schema.attributes.size()) {
            throw PssgParseError{fmt::format("pssg attribute references invalid schema element: {}", pssg_element_id)};
        }

        if (!pssg_attrib_value.has_value()) {
            throw PssgParseError{"pssg attribute has invalid value size"};
        }

        auto pssg_attrib = ScePssgAttribute{
            .id = pssg_attrib_id,
        };

        pssg_attrib.value.assign(pssg_attrib_value->begin(), pssg_attrib_value->end());
        pssg_file.elements[pssg_element_idx].attributes.emplace_back(std::move(pssg_attrib));
    }

    if (pssg_element_end_offs == reader.Position()) {
        return pssg_element_idx;
    } else if (pssg_element_end_offs < reader.Position()) {
        throw PssgParseError{"invalid pssg element, end offset < position"};
    }

    const auto pssg_data_buffer = reader.ReadBuffer(pssg_element_end_offs - reader.Position());
    if (!pssg_data_buffer.has_value()) {
        throw PssgParseError{"pssg element has invalid value size"};
    }

    // validate children
    // if children are invalid then treat this as a data element
    const auto &pssg_schema_element = pssg_schema.elements[pssg_element_id];
    auto has_valid_subtree = true;

    if (pssg_schema_element.type != ScePssgElementType::eNone &&
        pssg_schema_element.type != ScePssgElementType::eUnknown) {
        has_valid_subtree = false;
    }

    if (has_valid_subtree) {
        has_valid_subtree = CheckChildren(pssg_data_buffer.value(), pssg_schema);
    }

    if (has_valid_subtree) {
        try {
            util::bytes::BinaryReader child_reader{pssg_data_buffer.value()};
            for (; child_reader.Remaining() > 0;) {
                auto pssg_child_idx = ReadElement(child_reader, pssg_schema, pssg_file, pssg_element_idx);
                pssg_file.elements[pssg_element_idx].children.emplace_back(pssg_child_idx);
            }
        } catch (const std::exception &) {
            has_valid_subtree = false;
        }
    }

    if (!has_valid_subtree) {
        // element has no valid children, simply store what we got as data
        pssg_file.elements[pssg_element_idx].value.assign(pssg_data_buffer->begin(), pssg_data_buffer->end());
    }

    return pssg_element_idx;
}

auto LoadFromMemory(const std::span<const uint8_t> buffer, const PssgLogger *logger) -> ScePssgFile {
    PssgConsoleLogger console_logger;
    if (!logger) {
        logger = &console_logger;
    }

    util::bytes::BinaryReader reader{buffer};
    const auto pssg_magic = AssertRead<uint32_t>(reader);

    if (pssg_magic != kPssgMagicNumber) {
        throw PssgParseError{"magic number mismatch"};
    }

    const auto pssg_size = AssertRead<uint32_t>(reader);
    if (buffer.size_bytes() < pssg_size) {
        throw PssgParseError{fmt::format(
            "pssg file reports size of {} but the buffer has only {} bytes", pssg_size, buffer.size_bytes())};
    }

    ScePssgFile pssg_file;
    pssg_file.schema = ReadSchema(reader);
    pssg_file.root_element_idx = ReadElement(reader, pssg_file.schema, pssg_file, std::nullopt);

    return pssg_file;
}

auto CheckHeader(std::span<const uint8_t> buffer, [[maybe_unused]] const PssgLogger *logger) -> bool {
    util::bytes::BinaryReader reader{buffer};

    try {
        const auto pssg_magic = AssertRead<uint32_t>(reader);

        if (pssg_magic != kPssgMagicNumber) {
            return false;
        }
    } catch (const std::exception &) {
        return false;
    }

    return true;
}

} // namespace pssg
