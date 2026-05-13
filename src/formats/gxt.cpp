#include <stdexcept>
#include <fmt/format.h>

#include "formats/gxt.hpp"
#include "formats/gxp.hpp"
#include "formats/pspgu.hpp"

#include "common/binaryreader.hpp"

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
auto UnswizzleBitmap(std::span<const uint8_t> buffer, uint32_t stride, uint32_t width, uint32_t height)
    -> std::vector<uint8_t> {
    constexpr uint32_t kBlockWidth = 16;
    constexpr uint32_t kBlockHeight = 8;
    constexpr uint32_t kBlockSize = kBlockWidth * kBlockHeight;

    const uint32_t block_stride = width / kBlockWidth;
    const uint32_t num_blocks = width * height / kBlockSize; // how many sequenced blocks we have in this texture

    std::vector<uint8_t> bitmap;
    bitmap.resize(stride * height);

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

            bitmap[(y * stride) + x] = buffer[i++];
        }
    }

    return bitmap;
}

auto AdjustBitmap(std::span<const uint8_t> buffer, uint32_t stride, uint32_t width, uint32_t height)
    -> std::vector<uint8_t> {
    std::vector<uint8_t> bitmap;
    bitmap.resize(stride * height);

    for (uint32_t row_idx = 0; row_idx < height; ++row_idx) {
        for (uint32_t col_idx = 0; col_idx < stride && col_idx < width; ++col_idx) {
            const uint32_t src_idx = width * row_idx + col_idx;
            const uint32_t dst_idx = stride * row_idx + col_idx;

            bitmap[dst_idx] = buffer[src_idx];
        }
    }

    return bitmap;
}

class GxtConsoleLogger final : public GxtLogger {
public:
    auto log(std::string_view log_message) const -> void override {
        fmt::println("[libgxt console log] {}", log_message);
    }
};

class GxtGxpLogger final : public gxp::GxpLogger {
public:
    GxtGxpLogger(const GxtLogger &logger) : logger_{logger} {}
    auto log(std::string_view log_message) const -> void override { logger_.log(log_message); }

private:
    const GxtLogger &logger_;
};

enum SceTexturePixelFormat {
    eScePFRGBA5650 = 0,
    eScePFRGBA5551 = 1,
    eScePFRGBA4444 = 2,
    eScePFRGBA8888 = 3,
    eScePFIDX4 = 4,
    eScePFIDX8 = 5,
    eScePFIDX16 = 6,
    eScePFIDX32 = 7,
    eScePFDXT1 = 8,
    eScePFDXT3 = 9,
    eScePFDXT5 = 10,
    eScePFCount,
};

enum SceTextureClutFormat {
    eSceCLUTRGBA5650 = 0,
    eSceCLUTRGBA5551 = 1,
    eSceCLUTRGBA4444 = 2,
    eSceCLUTRGBA8888 = 3,
    eSceCLUTCount,
};

constexpr auto GetPixelBitWidth(SceTexturePixelFormat format) -> uint32_t {
    switch (format) {
    case eScePFRGBA5650:
    case eScePFRGBA5551:
    case eScePFRGBA4444:
        return 16;

    case eScePFRGBA8888:
        return 32;

    case eScePFIDX4:
        return 4;

    case eScePFIDX8:
        return 8;

    case eScePFIDX16:
        return 16;

    case eScePFIDX32:
        return 32;

    case eScePFDXT1:
    case eScePFDXT3:
    case eScePFDXT5:
    default:
        throw GxtParseError{"dxt pixel formats are unsupported"};
    }
}

auto ImageDecodeCLUT(
    const std::vector<RGBA8888Color> &palette, SceTexturePixelFormat format, std::span<const uint8_t> buffer,
    uint32_t width, uint32_t height) -> std::vector<uint8_t> {
    std::vector<uint8_t> rgba_plane;
    rgba_plane.resize(width * height * 4);

    switch (format) {
    case eScePFIDX4: {
        const auto stride = (width + 1) / 2;

        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width / 2; ++x) {
                const auto src_idx = stride * y + x;
                const auto dst_idx = 4 * (width * y + (2 * x));

                const auto pal_idx1 = buffer[src_idx] & 0xf;
                const auto pal_idx2 = (buffer[src_idx] >> 4) & 0xf;

                rgba_plane[dst_idx + 0] = palette[pal_idx1].r;
                rgba_plane[dst_idx + 1] = palette[pal_idx1].g;
                rgba_plane[dst_idx + 2] = palette[pal_idx1].b;
                rgba_plane[dst_idx + 3] = palette[pal_idx1].a;

                rgba_plane[dst_idx + 4] = palette[pal_idx2].r;
                rgba_plane[dst_idx + 5] = palette[pal_idx2].g;
                rgba_plane[dst_idx + 6] = palette[pal_idx2].b;
                rgba_plane[dst_idx + 7] = palette[pal_idx2].a;
            }

            if (width % 2 == 1) {
                const auto src_idx = (stride * y) + (width / 2);
                const auto dst_idx = 4 * (stride * y + width - 1);
                const auto pal_idx = buffer[src_idx] & 0xf;

                rgba_plane[dst_idx + 0] = palette[pal_idx].r;
                rgba_plane[dst_idx + 1] = palette[pal_idx].g;
                rgba_plane[dst_idx + 2] = palette[pal_idx].b;
                rgba_plane[dst_idx + 3] = palette[pal_idx].a;
            }
        }
        break;
    }
    case eScePFIDX8:
        for (uint32_t i = 0; i < buffer.size(); ++i) {
            const auto palette_idx = buffer[i];
            rgba_plane[4 * i + 0] = palette[palette_idx].r;
            rgba_plane[4 * i + 1] = palette[palette_idx].g;
            rgba_plane[4 * i + 2] = palette[palette_idx].b;
            rgba_plane[4 * i + 3] = palette[palette_idx].a;
        }
        break;
    case eScePFIDX16:
        for (uint32_t i = 0; i < buffer.size(); i = i + 2) {
            const auto palette_idx = uint16_t{buffer[i]} | (uint16_t{buffer[i + 1]} << 8);
            rgba_plane[4 * i + 0] = palette[palette_idx].r;
            rgba_plane[4 * i + 1] = palette[palette_idx].g;
            rgba_plane[4 * i + 2] = palette[palette_idx].b;
            rgba_plane[4 * i + 3] = palette[palette_idx].a;
        }
        break;
    case eScePFIDX32:
        for (uint32_t i = 0; i < buffer.size(); i = i + 4) {
            const auto palette_idx = uint32_t{buffer[i]} | (uint32_t{buffer[i + 1]} << 8) |
                                     (uint32_t{buffer[i + 2]} << 16) | (uint32_t{buffer[i + 3]} << 24);
            rgba_plane[4 * i + 0] = palette[palette_idx].r;
            rgba_plane[4 * i + 1] = palette[palette_idx].g;
            rgba_plane[4 * i + 2] = palette[palette_idx].b;
            rgba_plane[4 * i + 3] = palette[palette_idx].a;
        }
        break;
    default:
        throw GxtParseError{fmt::format("cannot decode clut for non clut format {}", static_cast<uint32_t>(format))};
        break;
    }

    return rgba_plane;
}

auto LoadPalette(
    const util::bytes::BinaryReader &parent_reader, SceTextureClutFormat format, uint32_t num_colors, uint32_t offset)
    -> std::vector<RGBA8888Color> {
    using Reader = util::bytes::BinaryReader;
    util::bytes::BinaryReader reader{parent_reader};
    AssertSeek(reader, offset);

    std::vector<RGBA8888Color> palette;
    palette.reserve(num_colors);

    const auto fnReadColor = [&]() -> glm::fvec4 (*)(Reader &) {
        switch (format) {
        case eSceCLUTRGBA5650:
            return [](Reader &r) {
                const auto rgba = AssertRead<uint16_t>(r);
                return pspgu::ColorPF5650ToRGBA8(rgba);
            };
        case eSceCLUTRGBA5551:
            return [](Reader &r) {
                const auto rgba = AssertRead<uint16_t>(r);
                return pspgu::ColorPF5551ToRGBA8(rgba);
            };
        case eSceCLUTRGBA4444:
            return [](Reader &r) {
                const auto rgba = AssertRead<uint16_t>(r);
                return pspgu::ColorPF4444ToRGBA8(rgba);
            };
        case eSceCLUTRGBA8888:
            return [](Reader &r) {
                const auto rgba = AssertRead<uint32_t>(r);
                return pspgu::ColorPF8888ToRGBA8(rgba);
            };
        default:
            throw GxtParseError{fmt::format("unsupported clut color format {}", static_cast<uint32_t>(format))};
        }
    }();

    for (uint32_t i = 0; i < num_colors; ++i) {
        const auto color_float = fnReadColor(reader);
        palette.emplace_back(
            RGBA8888Color{
                static_cast<uint8_t>(color_float.r * 255), static_cast<uint8_t>(color_float.g * 255),
                static_cast<uint8_t>(color_float.b * 255), static_cast<uint8_t>(color_float.a * 255)});
    }

    return palette;
}

auto ImageDecode(SceTexturePixelFormat pixel_format, std::span<const uint8_t> buffer, uint32_t width, uint32_t height)
    -> std::vector<uint8_t> {
    using Reader = util::bytes::BinaryReader;

    std::vector<uint8_t> rgba_plane;
    rgba_plane.resize(width * height * 4);

    const auto fnReadColor = [&]() -> glm::fvec4 (*)(Reader &) {
        switch (pixel_format) {
        case eScePFRGBA5650:
            return [](Reader &r) {
                const auto rgba = AssertRead<uint16_t>(r);
                return pspgu::ColorPF5650ToRGBA8(rgba);
            };
        case eScePFRGBA5551:
            return [](Reader &r) {
                const auto rgba = AssertRead<uint16_t>(r);
                return pspgu::ColorPF5551ToRGBA8(rgba);
            };
        case eScePFRGBA4444:
            return [](Reader &r) {
                const auto rgba = AssertRead<uint16_t>(r);
                return pspgu::ColorPF4444ToRGBA8(rgba);
            };
        case eScePFRGBA8888:
            return [](Reader &r) {
                const auto rgba = AssertRead<uint32_t>(r);
                return pspgu::ColorPF8888ToRGBA8(rgba);
            };
        default:
            throw GxtParseError{fmt::format("unsupported texel color format {}", static_cast<uint32_t>(pixel_format))};
        }
    }();

    Reader reader{buffer};
    for (uint32_t i = 0; i < width * height; ++i) {
        const auto color_float = fnReadColor(reader);
        rgba_plane.emplace_back(static_cast<uint8_t>(color_float.r * 255));
        rgba_plane.emplace_back(static_cast<uint8_t>(color_float.g * 255));
        rgba_plane.emplace_back(static_cast<uint8_t>(color_float.b * 255));
        rgba_plane.emplace_back(static_cast<uint8_t>(color_float.a * 255));
    }

    return rgba_plane;
}

auto LoadBitmap(
    const gxp::SceGxpFile &gxp_file, const gxp::SceGxpMotionFrame &motion_frame,
    const util::bytes::BinaryReader &parent_reader, const GxtLogger &logger, std::vector<GxtImageBitmap> &out_bitmaps)
    -> void {
    GxtImageBitmap bitmap;

    util::bytes::BinaryReader command_reader{parent_reader};
    AssertSeek(command_reader, motion_frame.ge_buffer_offs);

    struct {
        SceTexturePixelFormat texture_format = eScePFRGBA5650;
        SceTextureClutFormat clut_format = eSceCLUTRGBA5650;

        bool multi_clut_mode = false;
        bool high_speed_mode = false;
        bool dxt_ext = false;

        uint32_t max_lod = 0;
        uint32_t texture_stride = 0;
        uint32_t texture_addr = 0;
        uint32_t texture_width = 0;
        uint32_t texture_height = 0;
        uint32_t clut_addr = 0;
        uint32_t clut_size = 0;

        glm::fvec2 uv_scale = glm::fvec2{1.0f, 1.0f};
        glm::fvec2 uv_offset = glm::fvec2{0.0f, 0.0f};

        bool returned = false;
    } ge_state;

    for (uint32_t command_idx = 0; command_idx < motion_frame.ge_buffer_size / sizeof(uint32_t); ++command_idx) {
        const auto ge_command = AssertRead<uint32_t>(command_reader);
        const auto ge_command_opc = (ge_command & 0xff000000u) >> 24;
        const auto ge_command_arg = (ge_command & 0x00ffffffu);

        switch (ge_command_opc) {
        case pspgu::TEX_MODE: {
            const auto high_speed_mode = (ge_command_arg & 0x000000ffu);      // swizzling
            const auto multi_clut_mode = (ge_command_arg & 0x0000ff00u) >> 8; // not supported
            const auto max_lod = ge_command_arg & (0x00ff0000u) >> 16;

            if (high_speed_mode) {
                ge_state.high_speed_mode = true;
            } else {
                ge_state.high_speed_mode = false;
            }

            if (multi_clut_mode) {
                ge_state.multi_clut_mode = true;
            } else {
                ge_state.multi_clut_mode = false;
            }

            ge_state.max_lod = max_lod; // will be ignored regardless but is stored for completeness sake
            break;
        }

        case pspgu::TEX_FORMAT: {
            const auto texture_format = (ge_command_arg & 0x000000ffu); // pixel format
            const auto dxt_ext_enable = (ge_command_arg & 0x0000ff00u) >> 8;

            if (texture_format < eScePFCount) {
                ge_state.texture_format = static_cast<SceTexturePixelFormat>(texture_format);
            } else {
                logger.log(fmt::format("warning: incorrect pixel format set {}", texture_format));
            }

            if (dxt_ext_enable) {
                logger.log(fmt::format("warning: dxt ext set to true but is not implemented"));
                ge_state.dxt_ext = true;
            } else {
                ge_state.dxt_ext = false;
            }
            break;
        }

        case pspgu::TEX_BUF_WIDTH0: {
            const auto buffer_width = (ge_command_arg & 0x0000ffffu);
            const auto tex_addr_high = (ge_command_arg & 0x00ff0000u) >> 16;

            ge_state.texture_stride = buffer_width;
            ge_state.texture_addr = (ge_state.texture_addr & 0x00ffffff) | (tex_addr_high << 24);
            break;
        }

        case pspgu::TEX_ADDR0: {
            const auto tex_addr_low = (ge_command_arg & 0x00ffffff);
            ge_state.texture_addr = (ge_state.texture_addr & 0xff000000) | tex_addr_low;
            break;
        }

        case pspgu::TEX_SIZE0: {
            // the layout will look like this:
            // OOOOOOOO........HHHHHHHHWWWWWWWW
            // where width = 2^W, height = 2^H

            const uint32_t width = 1 << (ge_command_arg & 0x000000ffu);
            const uint32_t height = 1 << ((ge_command_arg & 0x0000ff00u) >> 8);

            ge_state.texture_width = width;
            ge_state.texture_height = height;
            break;
        }

        case pspgu::NOP:
        case pspgu::TEX_FLUSH:
            break;

        case pspgu::CLUT_FORMAT: {
            const auto clut_format = ge_command_arg & 3; // only 3 bits
            if (clut_format < eSceCLUTCount) {
                ge_state.clut_format = static_cast<SceTextureClutFormat>(clut_format);
            } else {
                logger.log(fmt::format("warning: incorrect CLUT format set {}", clut_format));
            }

            break;
        }

        case pspgu::CLUT_BUF_WIDTH: {
            const auto clut_addr_high = (ge_command_arg & 0x00ff0000u) >> 16;
            ge_state.clut_addr = (ge_state.clut_addr & 0x00ffffff) | (clut_addr_high << 24);
            break;
        }

        case pspgu::CLUT_BUF_PTR: {
            const auto clut_addr_low = (ge_command_arg & 0x00ffffffu);
            ge_state.clut_addr = (ge_state.clut_addr & 0xff000000) | clut_addr_low;
            break;
        }

        case pspgu::CLUT_LOAD: {
            const auto clut_num_colors = (ge_command_arg & 0x00ffffffu) * 8;
            ge_state.clut_size = clut_num_colors;
            break;
        }

        case pspgu::SceGeCommand::TEX_SCALE_U: {
            ge_state.uv_scale.x = util::bytes::F24ToF32(ge_command_arg & 0x00ffffff);
            break;
        }

        case pspgu::SceGeCommand::TEX_SCALE_V: {
            ge_state.uv_scale.y = util::bytes::F24ToF32(ge_command_arg & 0x00ffffff);
            break;
        }

        case pspgu::SceGeCommand::TEX_OFFSET_U: {
            ge_state.uv_offset.x = util::bytes::F24ToF32(ge_command_arg & 0x00ffffff);
            break;
        }

        case pspgu::SceGeCommand::TEX_OFFSET_V: {
            ge_state.uv_offset.y = util::bytes::F24ToF32(ge_command_arg & 0x00ffffff);
            break;
        }

        case pspgu::RET: {
            ge_state.returned = true;
            break;
        }

        default:
            logger.log(
                fmt::format("skip handling of GE command {:#04x} - opcode {:#010x}", ge_command, ge_command_opc));
            break;
        }
    }

    // validate the ge state and import the texture
    if (ge_state.multi_clut_mode) {
        logger.log("warning: multi-clut is not supported");
    }

    if (ge_state.dxt_ext) {
        logger.log("warning: dxt ext is not supported");
    }

    // a little bit of secret lore knowledge is needed here...
    const auto image_object_offs = gxp_file.object_offs;
    const auto image_width_offs = image_object_offs + 0x20;
    const auto image_height_offs = image_object_offs + 0x24;

    const auto verify_width = AssertReadAt<uint32_t>(command_reader, image_width_offs);
    const auto verify_height = AssertReadAt<uint32_t>(command_reader, image_height_offs);

    if (verify_width != ge_state.texture_width || verify_height != ge_state.texture_height) {
        logger.log(
            fmt::format(
                "warning: something is off! ge state reports {}x{}, gxt reports {}x{}, will use GE value",
                ge_state.texture_width, ge_state.texture_height, verify_width, verify_height));
    }

    if (ge_state.texture_width == 0 || ge_state.texture_height == 0) {
        throw GxtParseError{"invalid texture dimensions"};
    }

    logger.log(
        fmt::format(
            "GE simulator state machine:\n"
            "\ttexture_width: {}\n"
            "\ttexture_height: {}\n"
            "\ttexture_stride: {}\n"
            "\ttexture_offs: {}\n"
            "\tclut_addr: {}\n"
            "\tclut_size: {}\n"
            "\tswizzled: {}\n"
            "\tclut_format: {}\n"
            "\tpixel_format: {}\n",
            ge_state.texture_width, ge_state.texture_height, ge_state.texture_stride, ge_state.texture_addr,
            ge_state.clut_addr, ge_state.clut_size, ge_state.high_speed_mode,
            static_cast<uint32_t>(ge_state.clut_format), static_cast<uint32_t>(ge_state.texture_format)));

    const auto is_palette_enabled = ge_state.clut_addr != 0 && ge_state.clut_size != 0;
    const auto is_unswizzle_needed = ge_state.high_speed_mode;

    bitmap.width = ge_state.texture_width;
    bitmap.height = ge_state.texture_height;
    bitmap.uv_offset = ge_state.uv_offset;
    bitmap.uv_scale = ge_state.uv_scale;

    bool is_format_matching_clut = true;
    if (ge_state.clut_size != 0) {
        switch (ge_state.texture_format) {
        case eScePFIDX4: // max 0xf
            is_format_matching_clut = (ge_state.clut_size <= 0x10);
            break;
        case eScePFIDX8: // max 0xff
            is_format_matching_clut = (ge_state.clut_size <= 0x100);
            break;
        case eScePFIDX16: // max 0xffff
            is_format_matching_clut = (ge_state.clut_size <= 0x10000);
            break;
        case eScePFIDX32: // max 0xffffffff
            is_format_matching_clut = (ge_state.clut_size <= 0xffffffff);
            break;
        default:
            is_format_matching_clut = false;
            break;
        }
    }

    if (!is_format_matching_clut) {
        throw GxtParseError{fmt::format(
            "incorrect GE stat: clut size is {} but the pixel format is {}", ge_state.clut_size,
            static_cast<uint32_t>(ge_state.texture_format))};
    }

    // here we have to calculate a different stride
    // the texture stride set in GE command list is basically the width of the texture
    // for the unswizzle we need to have a BYTE WIDTH of the texture (as GE works on BYTES not TEXELS)
    // basically just get the size of a texel and multiply it by the width
    const auto texel_byte_width = GetPixelBitWidth(ge_state.texture_format);

    // assume byte = 8 bits for allegrex
    // we have to handle a case for textures of width 1xH and BPP < 8
    // this is the case for example for clnf00.gxx
    //
    // * |texture_byte_stride| - means the stride of the LOADED texture
    // * |texture_load_stride| - means the stride of the READ texture
    // e.g. for tips, the LOADED texture is 512x512, but the STORED texture size is 480x512
    // so in this case TBW0 = 480, TSIZE0 = 512x512
    // separate case is when the stored stride is larger, e.g. clnf00_04.gxt, then we get
    // LOADED texture of size 1x128, but the STORED texture is 32x128
    const auto texture_byte_stride = (ge_state.texture_width * texel_byte_width + 7) / 8;
    const auto texture_load_stride = ge_state.texture_stride * texel_byte_width / 8;

    AssertSeek(command_reader, ge_state.texture_addr);

    const auto ge_max_read_size = static_cast<uint32_t>(command_reader.Remaining());
    auto raw_bitmap =
        command_reader.ReadBuffer(std::min(ge_state.texture_height * texture_load_stride, ge_max_read_size));

    if (!raw_bitmap.has_value()) {
        throw GxtParseError{fmt::format("cannot read bitmap at specified ge address: {:#010x}", ge_state.texture_addr)};
    }

    std::vector<uint8_t> pixel_buffer;
    pixel_buffer.resize(texture_load_stride * ge_state.texture_height, 0);
    for (uint32_t line = 0; line < ge_state.texture_height; ++line) {
        const auto line_begin = texture_load_stride * line;
        const auto line_end = line_begin + texture_load_stride;
        const auto pixel_buffer_offs = texture_load_stride * line;

        if (line_end > raw_bitmap->size_bytes()) {
            logger.log("warning: GE was instruced to read out of bounds");
            break;
        }

        std::copy(
            raw_bitmap->begin() + line_begin, raw_bitmap->begin() + line_end, pixel_buffer.begin() + pixel_buffer_offs);
    }

    if (is_palette_enabled) {
        const auto palette = LoadPalette(command_reader, ge_state.clut_format, ge_state.clut_size, ge_state.clut_addr);
        if (is_unswizzle_needed) {
            const auto tex_buffer_stride = bitmap.rgba_plane = ImageDecodeCLUT(
                palette, ge_state.texture_format,
                UnswizzleBitmap(pixel_buffer, texture_byte_stride, texture_load_stride, ge_state.texture_height),
                ge_state.texture_width, ge_state.texture_height);
        } else {
            bitmap.rgba_plane = ImageDecodeCLUT(
                palette, ge_state.texture_format,
                AdjustBitmap(pixel_buffer, texture_byte_stride, texture_load_stride, ge_state.texture_height),
                ge_state.texture_width, ge_state.texture_height);
        }
    } else {
        if (is_unswizzle_needed) {
            bitmap.rgba_plane = ImageDecode(
                ge_state.texture_format,
                UnswizzleBitmap(pixel_buffer, texture_byte_stride, texture_load_stride, ge_state.texture_height),
                ge_state.texture_width, ge_state.texture_height);
        } else {
            bitmap.rgba_plane = ImageDecode(
                ge_state.texture_format,
                AdjustBitmap(pixel_buffer, texture_byte_stride, texture_load_stride, ge_state.texture_height),
                ge_state.texture_width, ge_state.texture_height);
        }
    }

    out_bitmaps.emplace_back(std::move(bitmap));
}

auto LoadBitmaps(std::span<const uint8_t> buffer, const GxtLogger *logger) -> std::vector<GxtImageBitmap> {
    GxtConsoleLogger console_logger;
    if (!logger) {
        logger = &console_logger;
    }

    GxtGxpLogger gxp_logger{*logger};
    const auto gxp_file = gxp::LoadFromMemory(buffer);

    if (gxp_file.format != kGxtStyle) {
        throw GxtParseError{"valid gxp file was detected but this is not a gxt flavor"};
    }

    util::bytes::BinaryReader reader{buffer};

    logger->log(
        fmt::format(
            "gxt file header:\n"
            "\tmagic: {:#010x}\n"
            "\tversion: {:#010x}\n"
            "\tstyle: {:#010x}\n"
            "\toption: {:#010x}\n",
            gxp_file.magic, gxp_file.version, gxp_file.format, gxp_file.option));

    if (gxp_file.root_object.num_motions == 0) {
        logger->log("warning: the gxp file has no motion frames so no textures were loaded");
        return {};
    } else if (gxp_file.root_object.num_motions > 0) {
        logger->log(fmt::format("warning: the gxp file has {} frames", gxp_file.root_object.num_motions));
    }

    std::vector<GxtImageBitmap> out_bitmaps;
    for (const auto &gxp_motion : gxp_file.root_object.motions) {
        for (const auto &gxp_motion_frame : gxp_motion.frames) {
            LoadBitmap(gxp_file, gxp_motion_frame, reader, *logger, out_bitmaps);
        }
    }

    return out_bitmaps;
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
