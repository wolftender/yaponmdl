#include <stdexcept>
#include <stack>
#include <map>

#include <fmt/format.h>

#include "common/binaryreader.hpp"
#include "formats/gxx.hpp"
#include "formats/pspgu.hpp"

// based on gxxtool from patamodding community

namespace gxx {

constexpr uint32_t kGxxMagicNumber = 0x2e475858;
constexpr uint32_t kGxxVersion = 0x302e3130;
constexpr uint32_t kGxxStyle = 0x2e474d4f;

struct GxxHeader {
    uint32_t magic;   // + 0x00
    uint32_t version; // + 0x04
    uint32_t style;   // + 0x08
    uint32_t option;  // + 0x0c
};

using GxxParseError = std::runtime_error;

template <typename T>
auto AssertRead(util::bytes::BinaryReader &reader, std::optional<std::string_view> message = std::nullopt) -> T {
    const auto value = reader.Read<T>();
    if (!value.has_value()) {
        if (message.has_value()) {
            throw GxxParseError{std::string{message.value()}};
        } else {
            throw GxxParseError{fmt::format("invalid byte read at position {}", reader.Position())};
        }
    }

    return value.value();
}

auto AssertSeek(util::bytes::BinaryReader &reader, uint64_t position) -> void {
    const auto res = reader.Seek(position);
    if (util::bytes::BinaryReader::Result::eSuccess != res) {
        throw GxxParseError{fmt::format("invalid gxt offset calculated {}", position)};
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
        const auto res = reader.Read<uint8_t>();
        if (!res.has_value()) {
            break;
        }

        chr = res.value();
        ++length;
    } while (chr);

    AssertSeek(reader, offset);
    const auto buffer = reader.ReadBuffer(length);

    if (!buffer.has_value()) {
        throw GxxParseError{fmt::format("cannot read string at {}", offset)};
    }

    return buffer.value();
}

auto ReadHeader(util::bytes::BinaryReader &reader) -> GxxHeader {
    GxxHeader header;
    header.magic = AssertRead<uint32_t>(reader);
    header.version = AssertRead<uint32_t>(reader);
    header.style = AssertRead<uint32_t>(reader);
    header.option = AssertRead<uint32_t>(reader);

    return header;
}

class GxxConsoleLogger final : public GxxLogger {
public:
    auto log(std::string_view log_message) const -> void override {
        fmt::println("[libgxx console log] {}", log_message);
    }
};

class GxxLoaderContext final {
public:
    GxxLoaderContext(const GxxLogger &logger, std::span<const uint8_t> buffer) : logger_{logger}, buffer_{buffer} {
        util::bytes::BinaryReader reader{buffer_};
        const auto header = ReadHeader(reader);

        if (header.magic != kGxxMagicNumber || header.version != kGxxVersion || header.style != kGxxStyle) {
            throw GxxParseError{"invalid gxx file"};
        }

        logger_.log(
            fmt::format(
                "gxx file header:\n"
                "\tmagic: {:#010x}\n"
                "\tversion: {:#010x}\n"
                "\tstyle: {:#010x}\n"
                "\toption: {:#010x}\n",
                header.magic, header.version, header.style, header.option));
    }

    auto LoadModel() -> GxxModel {
        util::bytes::BinaryReader reader{buffer_};
        GxxModel gxx_model;
        const auto model_section_offset = AssertReadAt<uint32_t>(reader, 0x10); // header length

        // clang-format off
        const auto texture_section_offset   = AssertReadAt<uint32_t>(reader, model_section_offset + 0x08);
        const auto num_textures             = AssertReadAt<uint32_t>(reader, model_section_offset + 0x0c);
        const auto anim_section_offset      = AssertReadAt<uint32_t>(reader, model_section_offset + 0x10);
        const auto num_animations           = AssertReadAt<uint32_t>(reader, model_section_offset + 0x14);
        const auto bone_section_offset      = AssertReadAt<uint32_t>(reader, model_section_offset + 0x18);
        const auto num_bones                = AssertReadAt<uint32_t>(reader, model_section_offset + 0x1c);
        // clang-format on

        logger_.log(
            fmt::format(
                "gxx file stats:\n"
                "\ttexture offset: {:#010x}\n"
                "\tanim offset: {:#010x}\n"
                "\tbone offset: {:#010x}\n"
                "\tnum textures: {}\n"
                "\tnum anims: {}\n"
                "\tnum bones: {}\n",
                texture_section_offset, anim_section_offset, bone_section_offset, num_textures, num_animations,
                num_bones));

        for (uint32_t gxx_texture_id = 0; gxx_texture_id < num_textures; ++gxx_texture_id) {
            const auto texture_entry_offset = texture_section_offset + (0x04 * gxx_texture_id);
            const auto texture_data_offset = AssertReadAt<uint32_t>(reader, texture_entry_offset);

            const auto texture_name_offset = AssertReadAt<uint32_t>(reader, texture_data_offset + 0x04);
            const auto texture_id_ptr_offset = AssertReadAt<uint32_t>(reader, texture_data_offset + 0x10);

            const auto texture_name_buffer = AssertReadStringAt(reader, texture_name_offset);
            const auto texture_id_offset = AssertReadAt<uint32_t>(reader, texture_id_ptr_offset);
            const auto texture_id = AssertReadAt<uint32_t>(reader, texture_id_offset);

            gxx_model.textures.emplace_back(
                GxxTexture{texture_id, std::string{texture_name_buffer.begin(), texture_name_buffer.end()}});
        }

        for (uint32_t gxx_bone_id = 0; gxx_bone_id < num_bones; ++gxx_bone_id) {
            const auto bone_entry_ofset = bone_section_offset + (0x08 * gxx_bone_id);
            const auto bone_name_offset = AssertReadAt<uint32_t>(reader, bone_entry_ofset + 0x04);
            const auto bone_name_buffer = AssertReadStringAt(reader, bone_name_offset);

            gxx_model.bones.emplace_back(GxxBone{std::string{bone_name_buffer.begin(), bone_name_buffer.end()}});
        }

        for (uint32_t gxx_anim_id = 0; gxx_anim_id < num_animations; ++gxx_anim_id) {
            const auto anim_entry_offset = anim_section_offset + (0x10 * gxx_anim_id);
            const auto anim_data_offset = AssertReadAt<uint32_t>(reader, anim_entry_offset);
            const auto anim_num_frames = AssertReadAt<uint32_t>(reader, anim_entry_offset + 0x04);
            const auto anim_framerate = AssertReadAt<float>(reader, anim_entry_offset + 0x08);
            const auto anim_frameloop = AssertReadAt<float>(reader, anim_entry_offset + 0x0c);
            const auto anim_params_offset = AssertReadAt<uint32_t>(reader, anim_data_offset);
            const auto anim_data_size = AssertReadAt<uint32_t>(reader, anim_params_offset - 0x38);

            GxxAnimation animation;
            animation.name = fmt::format("gxx_{:x}", anim_entry_offset);
            animation.num_frames = anim_num_frames;
            animation.frameloop = anim_frameloop;
            animation.framerate = anim_framerate;

            gxx_model.animations.emplace_back(std::move(animation));

            for (uint32_t gxx_frame_id = 0; gxx_frame_id < animation.num_frames; ++gxx_frame_id) {
                const auto anim_frame_offset = AssertReadAt<uint32_t>(reader, anim_data_offset + (0x04 * gxx_frame_id));
                const auto end_counter = anim_frame_offset + anim_data_size;

                ge_state_.program_counter = anim_frame_offset;

                logger_.log(
                    fmt::format("BEGIN FRAME: anim {} frame {} end {:#010x}", gxx_anim_id, gxx_frame_id, end_counter));
                while (ge_state_.program_counter < end_counter) {
                    const auto res = ParseAnimCommand(gxx_model, animation, reader);
                    if (res == ParseCommandResult::eComplete) {
                        break;
                    }
                }
            }
        }

        return gxx_model;
    }

    GxxLoaderContext(const GxxLoaderContext &) = delete;
    auto operator=(const GxxLoaderContext &) = delete;

    GxxLoaderContext(GxxLoaderContext &&) noexcept = delete;
    auto operator=(GxxLoaderContext &&) noexcept = delete;

private:
    enum class ParseCommandResult {
        eContinue,
        eComplete,
    };

    /// handy reference list for this funtion:
    /// - https://www.psdevwiki.com/psp/Graphics_Engine
    /// -
    auto ParseAnimCommand(GxxModel &gxx_model, GxxAnimation &gxx_animation, util::bytes::BinaryReader &reader)
        -> ParseCommandResult {
        const auto command = AssertReadAt<uint32_t>(reader, ge_state_.program_counter);
        const uint32_t ge_command = (0xff000000u & command) >> 24;

        switch (ge_command) {
        case pspgu::SceGeCommand::JUMP: {
            const auto jump_address = 0x00ffffff & command;
            ge_state_.program_counter = jump_address;
            return ParseCommandResult::eContinue; // don't increase PC after jump
        }

        case pspgu::SceGeCommand::CALL: {
            const auto jump_address = 0x00ffffff & command;

            ge_state_.callstack.push(ge_state_.program_counter + sizeof(uint32_t));
            ge_state_.program_counter = jump_address;
            return ParseCommandResult::eContinue; // don't increase PC after jump
        }

        case pspgu::SceGeCommand::RET: {
            if (ge_state_.callstack.empty()) {
                return ParseCommandResult::eComplete; // ret when empty stack -> done
            }

            ge_state_.program_counter = ge_state_.callstack.top();
            ge_state_.callstack.pop();
            return ParseCommandResult::eContinue; // don't increase PC after jump
        }

        case pspgu::SceGeCommand::VADDR: {
            const auto vert_address = 0x00ffffffu & command;
            if (vert_address > 0) {
                ge_state_.vtx_buffer_addr = vert_address;
            }

            break;
        }

        case pspgu::SceGeCommand::VERTEX_TYPE: {
            // clang-format off
            ge_state_.format_texture  = pspgu::ToTextureFormat(0b0011 & (command >> 0));
            ge_state_.format_color    = pspgu::ToColorFormat(0b0111 & (command >> 2));
            ge_state_.format_normal   = pspgu::ToNormalFormat(0b0011 & (command >> 5));
            ge_state_.format_position = pspgu::ToVertexFormat(0b0011 & (command >> 7));
            ge_state_.format_weight   = pspgu::ToWeightFormat(0b0011 & (command >> 9));
            ge_state_.format_index    = pspgu::ToIndexFormat(0b0011 & (command >> 11));

            auto num_weights        = 0b1111 & (command >> 14);
            auto num_morphs         = 0b1111 & (command >> 18);
            // clang-format on

            if (ge_state_.format_weight != pspgu::eSceGuFmtWeightNONE) {
                num_weights = num_weights + 1;
            } else {
                num_weights = 0;
            }

            if (num_morphs != 0) {
                throw GxxParseError{"vertex morphs are not supported"};
            }

            break;
        }

        case pspgu::SceGeCommand::WORLD_MATRIX_NUMBER: {
            // as per pspsdk: src/gu/sceGuSetMatrix.c
            // the next 3 * 4 commands *SHOULD BE* WORLD_MATRIX_DATA

            const auto world_mat_idx = 0x00ffffff & command;
            if (world_mat_idx != 0) {
                logger_.log("warning: GE command WORLD_MATRIX_NUMBER with index != 0");
            }

            std::array<float, 16> matrix_values;
            std::fill(matrix_values.begin(), matrix_values.end(), 0.0f);
            for (uint32_t i = 0; i < 4; ++i) {
                for (uint32_t j = 0; j < 3; ++j) {
                    ge_state_.program_counter = ge_state_.program_counter + sizeof(uint32_t);
                    const auto mtx_command = AssertReadAt<uint32_t>(reader, ge_state_.program_counter);

                    if (((mtx_command & 0xff000000) >> 24) != pspgu::SceGeCommand::WORLD_MATRIX_DATA) {
                        throw GxxParseError{"invalid gxx, expected 4x3 matrix data values after matrix number command"};
                    }

                    matrix_values[j + i * 4] = util::bytes::F24ToF32(mtx_command & 0x00ffffff);
                }
            }

            break;
        }

        case pspgu::SceGeCommand::WORLD_MATRIX_DATA: {
            throw GxxParseError{fmt::format(
                "invalid gxx, WORLD_MATRIX_DATA opcode at pc {:#010x} was unexpected", ge_state_.program_counter)};
        }

        case pspgu::SceGeCommand::TEX_SCALE_U: {
            ge_state_.uv_scale.x = util::bytes::F24ToF32(command & 0x00ffffff);
            break;
        }

        case pspgu::SceGeCommand::TEX_SCALE_V: {
            ge_state_.uv_scale.y = util::bytes::F24ToF32(command & 0x00ffffff);
            break;
        }

        case pspgu::SceGeCommand::TEX_OFFSET_U: {
            ge_state_.uv_offset.x = util::bytes::F24ToF32(command & 0x00ffffff);
            break;
        }

        case pspgu::SceGeCommand::TEX_OFFSET_V: {
            ge_state_.uv_offset.y = util::bytes::F24ToF32(command & 0x00ffffff);
            break;
        }

        case pspgu::SceGeCommand::TEXTURE_ENABLE: {
            const auto do_enable = 0x00ffffff & command;
            if (do_enable) {
                ge_state_.enable_texture_map = true;
            } else {
                ge_state_.enable_texture_map = false;
            }

            break;
        }

        case pspgu::SceGeCommand::BASE: {
            const auto base_addr = 0x00ffffff & command;
            if (base_addr != 0) {
                throw GxxParseError{"changing pc base is currently unsupported"};
            }

            break;
        }

        case pspgu::SceGeCommand::PRIM: {
            break;
        }

        case pspgu::SceGeCommand::NOP: {
            break;
        }

        case pspgu::SceGeCommand::AMBIENT_COLOR: {
            const auto b = uint8_t((0x00ff0000u & command) >> 16);
            const auto g = uint8_t((0x0000ff00u & command) >> 8);
            const auto r = uint8_t((0x000000ffu & command) >> 0);

            ge_state_.albedo_color.r = static_cast<float>(r) / 255.0f;
            ge_state_.albedo_color.g = static_cast<float>(g) / 255.0f;
            ge_state_.albedo_color.b = static_cast<float>(b) / 255.0f;

            break;
        }

        case pspgu::SceGeCommand::AMBIENT_ALPHA: {
            const auto a = uint8_t((0x000000ffu & command));
            ge_state_.albedo_color.a = static_cast<float>(a) / 255.0f;
            break;
        }

        default: {
            logger_.log(
                fmt::format(
                    "skip handling of GE command {:#04x} - opcode {:#010x} at pc {:#010x}", ge_command, command,
                    ge_state_.program_counter));
            break;
        }
        }

        ge_state_.program_counter = ge_state_.program_counter + sizeof(uint32_t);
        return ParseCommandResult::eContinue;
    }

    const GxxLogger &logger_;

    std::span<const uint8_t> buffer_;
    std::map<uint32_t, uint32_t> mesh_cache_;

    // ge state
    struct GEState {
        pspgu::SceGuFmtColor format_color = pspgu::eSceGuFmtColorNONE;
        pspgu::SceGuFmtNormal format_normal = pspgu::eSceGuFmtNormalNONE;
        pspgu::SceGuFmtTexture format_texture = pspgu::eSceGuFmtTextureNONE;
        pspgu::SceGuFmtVertex format_position = pspgu::eSceGuFmtVertexNONE;
        pspgu::SceGuFmtWeight format_weight = pspgu::eSceGuFmtWeightNONE;
        pspgu::SceGuFmtIndex format_index = pspgu::eSceGuFmtIndexNONE;
        uint32_t num_weights = 0;

        glm::fmat4x4 world_matrix = glm::fmat4x4{1.0f};
        std::array<glm::fmat4x4, 8> bone_matrix = {
            glm::fmat4x4{1.0f}, glm::fmat4x4{1.0f}, glm::fmat4x4{1.0f}, glm::fmat4x4{1.0f},
            glm::fmat4x4{1.0f}, glm::fmat4x4{1.0f}, glm::fmat4x4{1.0f}, glm::fmat4x4{1.0f},
        };

        uint32_t vtx_buffer_addr = 0;
        uint32_t current_texture = 0;

        bool enable_texture_map = false;

        glm::fvec2 uv_scale = glm::fvec2{1.0f, 1.0f};
        glm::fvec2 uv_offset = glm::fvec2{0.0f, 0.0f};

        glm::fvec4 albedo_color = glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f};

        std::stack<uint32_t> callstack;
        uint32_t program_counter;
    };

    GEState ge_state_;
};

auto LoadModelFromMemory(std::span<const uint8_t> buffer, const GxxLogger *logger) -> GxxModel {
    GxxConsoleLogger console_logger;
    if (!logger) {
        logger = &console_logger;
    }

    GxxLoaderContext context{*logger, buffer};
    return context.LoadModel();
}

auto CheckHeader(std::span<const uint8_t> buffer, [[maybe_unused]] const GxxLogger *logger) -> bool {
    util::bytes::BinaryReader reader{buffer};
    try {
        const auto header = ReadHeader(reader);
        if (header.magic != kGxxMagicNumber || header.version != kGxxVersion || header.style != kGxxStyle) {
            return false;
        }
    } catch (const std::exception &) {
        return false;
    }

    return true;
}

} // namespace gxx