#include <stdexcept>
#include <fmt/format.h>

#include "formats/gxt.hpp"
#include "common/binaryreader.hpp"

// based on https://github.com/owodzeg/GXTEdit/

namespace gxt {

constexpr uint32_t kGxtMagicNumber = 0x2e475858;
constexpr uint32_t kGxtVersion = 0x302e3130;
constexpr uint32_t kGxtStyle = 0x2e47494d;

enum GxtPaletteType {
    ePaletteTypeSize16 = 0x2,
    ePaletteTypeSize256 = 0x20,
};

struct GxtHeader {
    uint32_t magic;   // + 0x00
    uint32_t version; // + 0x04
    uint32_t style;   // + 0x08
    uint32_t option;  // + 0x0c
};

using GxtParseError = std::runtime_error;

template <typename T>
auto AssertRead(util::bytes::BinaryReader &reader, std::optional<std::string_view> message = std::nullopt) -> T {
    const auto value = reader.Read<T>();
    if (!value.has_value()) {
        if (message.has_value()) {
            throw GxtParseError{std::string{message.value()}};
        } else {
            throw GxtParseError{fmt::format("invalid byte read at position {}", reader.Position())};
        }
    }

    return value.value();
}

auto AssertSeek(util::bytes::BinaryReader &reader, uint64_t position) -> void {
    const auto res = reader.Seek(position);
    if (util::bytes::BinaryReader::Result::eSuccess != res) {
        throw GxtParseError{fmt::format("invalid gxt offset calculated {}", position)};
    }
}

template <typename T> auto AssertReadAt(util::bytes::BinaryReader &reader, uint64_t offset) -> T {
    AssertSeek(reader, offset);
    return AssertRead<T>(reader);
}

auto ReadHeader(util::bytes::BinaryReader &reader) -> GxtHeader {
    GxtHeader header;
    header.magic = AssertRead<uint32_t>(reader);
    header.version = AssertRead<uint32_t>(reader);
    header.style = AssertRead<uint32_t>(reader);
    header.option = AssertRead<uint32_t>(reader);

    return header;
}

struct RGBA8888Color {
    uint8_t r, g, b, a;
};

/*
 * guide to texture swizzling for dummies
 * based on ps2dev wiki
 *
 * normally the texture is laid out in the memory sequentially, i.e.
 * the texture has a certain stride (width * bpp), and has (height) rows
 * this is all good but the GE reads 16 byte * 8 row blocks
 * a swizzled texture has the blocks laid out sequentially
 */
auto UnswizzleBitmap(std::span<const uint8_t> buffer, uint32_t width, uint32_t height) -> std::vector<uint8_t> {
    constexpr uint32_t kBlockWidth = 16;
    constexpr uint32_t kBlockHeight = 8;
    constexpr uint32_t kBlockSize = kBlockWidth * kBlockHeight;

    const uint32_t block_stride = width / kBlockWidth;
    const uint32_t num_blocks = width * height / kBlockSize; // how many sequenced blocks we have in this texture

    std::vector<uint8_t> bitmap;
    bitmap.resize(width * height);

    uint32_t i = 0;
    for (uint32_t block_index = 0; block_index < num_blocks; ++block_index) {
        const uint32_t block_x = block_index % block_stride;
        const uint32_t block_y = block_index / block_stride;
        const uint32_t block_sx = block_x * kBlockWidth;
        const uint32_t block_sy = block_y * kBlockHeight;

        for (uint32_t byte_index = 0; byte_index < kBlockSize; ++byte_index) {
            const uint32_t byte_x = byte_index % kBlockWidth;
            const uint32_t byte_y = byte_index / kBlockWidth;
            const uint32_t x = block_sx + byte_x;
            const uint32_t y = block_sy + byte_y;

            bitmap[(y * width) + x] = buffer[i++];
        }
    }

    return bitmap;
}

auto ImageDecode(
    uint32_t palette_size, const std::vector<RGBA8888Color> &palette, std::span<const uint8_t> buffer, uint32_t width,
    uint32_t height) -> std::vector<uint8_t> {
    std::vector<uint8_t> rgba_plane;
    rgba_plane.resize(width * height * 4);

    if (palette_size > 0x10) {
        for (uint32_t i = 0; i < buffer.size(); ++i) {
            const auto palette_idx = buffer[i];
            rgba_plane[4 * i + 0] = palette[palette_idx].r;
            rgba_plane[4 * i + 1] = palette[palette_idx].g;
            rgba_plane[4 * i + 2] = palette[palette_idx].b;
            rgba_plane[4 * i + 3] = palette[palette_idx].a;
        }
    } else {
        for (uint32_t i = 0; i < buffer.size(); ++i) {
            const auto palette_idx1 = buffer[i] & 0xf;
            const auto palette_idx2 = (buffer[i] >> 4) & 0xf;
            rgba_plane[8 * i + 0] = palette[palette_idx1].r;
            rgba_plane[8 * i + 1] = palette[palette_idx1].g;
            rgba_plane[8 * i + 2] = palette[palette_idx1].b;
            rgba_plane[8 * i + 3] = palette[palette_idx1].a;

            rgba_plane[8 * i + 4] = palette[palette_idx2].r;
            rgba_plane[8 * i + 5] = palette[palette_idx2].g;
            rgba_plane[8 * i + 6] = palette[palette_idx2].b;
            rgba_plane[8 * i + 7] = palette[palette_idx2].a;
        }
    }

    return rgba_plane;
}

class GxtConsoleLogger final : public GxtLogger {
public:
    auto log(std::string_view log_message) const -> void override {
        fmt::println("[libgxt console log] {}", log_message);
    }
};

auto LoadBitmaps(std::span<const uint8_t> buffer, const GxtLogger *logger) -> std::vector<GxtImageBitmap> {
    GxtConsoleLogger console_logger;
    if (!logger) {
        logger = &console_logger;
    }

    util::bytes::BinaryReader reader{buffer};
    const auto header = ReadHeader(reader);

    if (header.magic != kGxtMagicNumber || header.version != kGxtVersion || header.style != kGxtStyle) {
        throw GxtParseError{"invalid gxt file"};
    }

    logger->log(
        fmt::format(
            "gxt file header:\n"
            "\tmagic: {:#010x}\n"
            "\tversion: {:#010x}\n"
            "\tstyle: {:#010x}\n"
            "\toption: {:#010x}\n",
            header.magic, header.version, header.style, header.option));

    constexpr uint64_t kOffsetImageParamOffset = 0x10;
    constexpr uint64_t kOffsetImageInfoOffset = 0x28;
    constexpr uint64_t kOffsetImagePlaneSize = 0x44;

    const auto image_param_offset = AssertReadAt<uint32_t>(reader, kOffsetImageParamOffset);
    const auto image_info_offset = AssertReadAt<uint32_t>(reader, kOffsetImageInfoOffset);
    const auto image_plane_size = AssertReadAt<uint32_t>(reader, kOffsetImagePlaneSize);
    const auto image_info_size = AssertReadAt<uint32_t>(reader, image_info_offset + 0x04);
    const auto image_swizzle = AssertReadAt<uint8_t>(reader, image_info_offset + 0x40);
    const auto image_palette_type = AssertReadAt<uint8_t>(reader, image_info_offset + image_info_size + 0x38);
    const auto image_palette_offset =
        AssertReadAt<uint32_t>(reader, image_info_offset + image_info_size + 0x34) & 0x00ffffff;
    const auto image_width = AssertReadAt<uint32_t>(reader, image_param_offset + 0x20);
    const auto image_height = AssertReadAt<uint32_t>(reader, image_param_offset + 0x24);
    const auto image_plane_offset = image_palette_offset - 0x40 - image_plane_size;

    uint32_t palette_size = 0;
    switch (image_palette_type) {
    case ePaletteTypeSize16:
        palette_size = 16;
        break;
    case ePaletteTypeSize256:
        palette_size = 256;
        break;
    default:
        throw GxtParseError{fmt::format("invalid gxt palette type {}", image_palette_type)};
    }

    std::vector<RGBA8888Color> palette;
    palette.resize(palette_size);

    AssertSeek(reader, image_palette_offset);
    for (uint32_t i = 0; i < palette_size; ++i) {
        palette[i].r = AssertRead<uint8_t>(reader);
        palette[i].g = AssertRead<uint8_t>(reader);
        palette[i].b = AssertRead<uint8_t>(reader);
        palette[i].a = AssertRead<uint8_t>(reader);
    }

    GxtImageBitmap bitmap;
    bitmap.width = image_width;
    bitmap.height = image_height;

    AssertSeek(reader, image_plane_offset);
    const auto encoded_plane = reader.ReadBuffer(image_plane_size);
    if (!encoded_plane.has_value()) {
        throw GxtParseError{fmt::format("cannot read encoded plane")};
    }

    if (image_swizzle) {
        const auto stride = palette_size == 256 ? image_width : image_width / 2;
        bitmap.rgba_plane = ImageDecode(
            palette_size, palette, UnswizzleBitmap(encoded_plane.value(), stride, image_height), image_width,
            image_height);
    } else {
        bitmap.rgba_plane = ImageDecode(palette_size, palette, encoded_plane.value(), image_width, image_height);
    }

    return {bitmap};
}

auto CheckHeader(std::span<const uint8_t> buffer, [[maybe_unused]] const GxtLogger *logger) -> bool {
    util::bytes::BinaryReader reader{buffer};
    try {
        const auto header = ReadHeader(reader);
        if (header.magic != kGxtMagicNumber || header.version != kGxtVersion || header.style != kGxtStyle) {
            return false;
        }
    } catch (const std::exception &) {
        return false;
    }

    return true;
}

} // namespace gxt
