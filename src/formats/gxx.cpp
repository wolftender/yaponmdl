#include <stdexcept>
#include <stack>
#include <map>

#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>

#include "common/binaryreader.hpp"

#include "formats/gxx.hpp"
#include "formats/gxp.hpp"
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

constexpr auto kGxxByteOrder = util::bytes::BinaryReader::ByteOrder::eLittleEndian;

template <typename T>
auto AssertRead(util::bytes::BinaryReader &reader, std::optional<std::string_view> message = std::nullopt) -> T {
    const auto value = reader.Read<kGxxByteOrder, T>();
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
        const auto res = reader.Read<kGxxByteOrder, uint8_t>();
        if (!res.has_value()) {
            break;
        }

        chr = res.value();
        ++length;
    } while (chr);

    AssertSeek(reader, offset);
    const auto buffer = reader.ReadBuffer(length - 1);

    if (!buffer.has_value()) {
        throw GxxParseError{fmt::format("cannot read string at {}", offset)};
    }

    return buffer.value();
}

template <typename T>
auto AssertReadAligned(util::bytes::BinaryReader &reader, std::optional<std::string_view> message = std::nullopt) -> T {
    const auto value = reader.ReadAligned<kGxxByteOrder, T>();
    if (!value.has_value()) {
        if (message.has_value()) {
            throw GxxParseError{std::string{message.value()}};
        } else {
            throw GxxParseError{fmt::format("invalid byte read at position {}", reader.Position())};
        }
    }

    return value.value();
}

template <uint32_t D> auto AssertReadAlignedFVecN(util::bytes::BinaryReader &reader) -> glm::vec<D, float> {
    glm::vec<D, float> val;
    for (uint32_t i = 0; i < D; ++i) {
        val[i] = static_cast<float>(AssertReadAligned<float>(reader));
    }

    return val;
};

template <typename T, uint32_t D> auto AssertReadAlignedFVecN(util::bytes::BinaryReader &reader) -> glm::vec<D, float> {
    constexpr float kRange = static_cast<float>(std::numeric_limits<T>::max());
    glm::vec<D, float> val;
    for (uint32_t i = 0; i < D; ++i) {
        val[i] = static_cast<float>(AssertReadAligned<T>(reader)) / kRange;
    }

    return val;
};

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

class GxxGxpLogger final : public gxp::GxpLogger {
public:
    GxxGxpLogger(const GxxLogger &logger) : logger_{logger} {}
    auto log(std::string_view log_message) const -> void override { logger_.log(log_message); }

private:
    const GxxLogger &logger_;
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
        GxxGxpLogger gxp_logger{logger_};
        gxp::SceGxpFile gxp_file = gxp::LoadFromMemory(buffer_, &gxp_logger);

        util::bytes::BinaryReader reader{buffer_};
        GxxModel gxx_model;

        logger_.log(
            fmt::format(
                "gxx file stats:\n"
                "\tnum_children: {}\n"
                "\tnum_nodes: {}\n"
                "\tnum_motions: {}\n",
                gxp_file.root_object.num_children, gxp_file.root_object.num_nodes, gxp_file.root_object.num_motions));

        const auto &root_object = gxp_file.root_object;
        for (uint32_t gxx_child_id = 0; gxx_child_id < root_object.num_children; ++gxx_child_id) {
            const auto &child_object = root_object.children[gxx_child_id];
            // how textures are supported by gxx:
            // the main object has child objects, type 10 signifies they are a texture
            // basically, as with gxt, a texture has motion frames with sampler setup
            // but they also have a name matching the texture, here we will skip the sampler
            // setup code and just assume the texture is loaded from the file with name
            // mathcing the subobject, this is why JUMP instruction is "texture load" actually

            if (child_object.type != 10) {
                logger_.log(
                    fmt::format("warning: gxx has a child object of type {} that will be skipped", child_object.type));
                continue;
            }

            if (child_object.num_motions != 1) {
                // TODO: completely theoretically, there is a VALID configuration here we can support
                // suppose the texture has N frames, each frame has its own command buffer offset,
                // actually, nothing is stopping the GXX GE command section to just jump to any texture here :p
                // this is unsupported as I have not found a model that uses such configuration yet
                logger_.log(
                    fmt::format(
                        "warning: gxx has a child object that is a texture and has {} motions - unsupported "
                        "configuration",
                        child_object.num_motions));
                continue;
            }

            const auto &texture_frame = child_object.motions.front();
            if (texture_frame.num_frames != 1) {
                logger_.log(
                    fmt::format(
                        "warning: gxx has a child object that is a texture and has {} frames - unsupported "
                        "configuration",
                        texture_frame.num_frames));
                continue;
            }

            const auto texture_command_offs = texture_frame.frames.front().ge_buffer_offs;
            gxx_model.textures.emplace_back(GxxTexture{texture_command_offs, child_object.name});

            texture_cache_.emplace(
                std::make_pair(texture_command_offs, static_cast<uint32_t>(gxx_model.textures.size() - 1)));

            logger_.log(
                fmt::format(
                    "gxx references a child texture {} with command buffer at offset {:#010x}", child_object.name,
                    texture_command_offs));
        }

        for (const auto &gxx_node : root_object.nodes) {
            GxxNode node;
            node.name = gxx_node.name;
            node.ge_matrix_offs = gxx_node.matrix_offs;

            logger_.log(
                fmt::format("gxx node with name={} and GE matrix offset {:#010x}", node.name, node.ge_matrix_offs));

            gxx_model.nodes.emplace_back(node);
        }

        for (const auto &gxx_motion : root_object.motions) {
            GxxAnimation animation;
            animation.name = fmt::format("gxx_{:x}", gxx_motion.offset);
            animation.framerate = gxx_motion.framerate;
            animation.num_frames = gxx_motion.num_frames;

            for (uint32_t gxx_frame_id = 0; gxx_frame_id < animation.num_frames; ++gxx_frame_id) {
                const auto &gxx_frame = gxx_motion.frames[gxx_frame_id];

                const auto ge_buffer_offset = gxx_frame.ge_buffer_offs;
                const auto ge_buffer_capacity = gxx_frame.ge_buffer_size;
                const auto ge_end_offset = ge_buffer_offset + ge_buffer_capacity;

                ge_state_.program_counter = ge_buffer_offset;
                logger_.log(
                    fmt::format(
                        "BEGIN FRAME: anim {} frame {} buffer {:#010x} ~ {:#010x}", animation.name, gxx_frame_id,
                        ge_buffer_offset, ge_end_offset));

                GxxAnimationFrame frame;
                while (ge_state_.program_counter < ge_end_offset) {
                    const auto res = ParseAnimCommand(gxx_model, frame, reader);

                    // if last return was reached, return the control
                    if (res == ParseCommandResult::eComplete) {
                        break;
                    }
                }

                // but its not over yet
                // we need to fetch the matrices now for the bones using their offsets
                const auto num_nodes = gxx_model.nodes.size();
                frame.node_matrices.reserve(num_nodes);

                for (uint32_t gxx_node_idx = 0; gxx_node_idx < num_nodes; ++gxx_node_idx) {
                    const auto &gxx_node = gxx_model.nodes[gxx_node_idx];

                    AssertSeek(reader, ge_buffer_offset + gxx_node.ge_matrix_offs);
                    std::array<float, 16> matrix_values;
                    std::fill(matrix_values.begin(), matrix_values.end(), 0.0f);
                    for (uint32_t i = 0; i < 4; ++i) {
                        for (uint32_t j = 0; j < 3; ++j) {
                            const uint32_t ge_command = AssertRead<uint32_t>(reader);
                            const uint32_t ge_opcode = (0xff000000 & ge_command) >> 24;
                            const uint32_t ge_param = (0x00ffffff & ge_command);

                            if (ge_opcode != pspgu::SceGeCommand::WORLD_MATRIX_DATA) {
                                logger_.log(
                                    fmt::format(
                                        "warning: cannot read bone {} ({}) matrix in motion {} frame {} at offset "
                                        "{:#010x}, ge opcode is {:#010x}",
                                        gxx_node.name, gxx_node_idx, animation.name, gxx_frame_id,
                                        gxx_node.ge_matrix_offs + i * sizeof(uint32_t), ge_command));
                                break;
                            }

                            matrix_values[j + i * 4] = util::bytes::F24ToF32(ge_param);
                        }
                    }

                    matrix_values[3 + 3 * 4] = 1.0f; // no perspective
                    frame.node_matrices.emplace_back(glm::make_mat4(matrix_values.data()));
                }

                if (frame.list.empty()) {
                    logger_.log(
                        fmt::format(
                            "warning: anim {} frame {} has no drawlists submitted", animation.name, gxx_frame_id));
                }

                // all commands were parsed, now we can combine them into a "frame"
                animation.frames.emplace_back(std::move(frame));

                // reset ge state
                ge_state_ = GEState{};
            }

            gxx_model.animations.emplace_back(std::move(animation));
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
    auto ParseAnimCommand(GxxModel &gxx_model, GxxAnimationFrame &frame, util::bytes::BinaryReader &reader)
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

            const auto tex_iter = texture_cache_.find(jump_address);
            if (tex_iter != texture_cache_.end()) {
                ge_state_.texture = tex_iter->second;
            }

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

            ge_state_.num_weights = num_weights;
            ge_state_.num_morphs = num_morphs;

            break;
        }

        case pspgu::SceGeCommand::BONE_MATRIX_NUMBER: {
            // check pspsdk: src/gu/sceGuBoneMatrix.c
            // unsigned int offset = ((index << 1) + index) << 2; - inverse of this operation
            const auto bone_mat_idx = (0x00ffffff & command) / 12;

            if (bone_mat_idx >= 8) {
                throw GxxParseError{fmt::format("GE command BONE_MATRIX_NUMBER with index {} >= 8", bone_mat_idx)};
            }

            std::array<float, 16> matrix_values;
            std::fill(matrix_values.begin(), matrix_values.end(), 0.0f);
            for (uint32_t i = 0; i < 4; ++i) {
                for (uint32_t j = 0; j < 3; ++j) {
                    ge_state_.program_counter = ge_state_.program_counter + sizeof(uint32_t);
                    const auto mtx_command = AssertReadAt<uint32_t>(reader, ge_state_.program_counter);

                    if (((mtx_command & 0xff000000) >> 24) != pspgu::SceGeCommand::BONE_MATRIX_DATA) {
                        throw GxxParseError{"invalid gxx, expected 4x3 matrix data values after matrix number command"};
                    }

                    matrix_values[j + i * 4] = util::bytes::F24ToF32(mtx_command & 0x00ffffff);
                }
            }

            matrix_values[3 + 3 * 4] = 1.0f; // no perspective
            ge_state_.bone_matrix[bone_mat_idx] = glm::make_mat4(matrix_values.data());
            break;
        }

        case pspgu::SceGeCommand::WORLD_MATRIX_NUMBER: {
            // as per pspsdk: src/gu/sceGuSetMatrix.c
            // the next 3 * 4 commands *SHOULD BE* WORLD_MATRIX_DATA

            if (ge_state_.mtx_unused_flag) {
                logger_.log("warning: GE command WORLD_MATRIX_NUMBER befure previous matrix was used");
            }

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

            matrix_values[3 + 3 * 4] = 1.0f; // no perspective
            ge_state_.world_matrix = glm::make_mat4(matrix_values.data());
            ge_state_.mtx_unused_flag = true;

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

        case pspgu::SceGeCommand::IADDR: {
            throw GxxParseError{"indexed primitives are currently unsupported"};
        }

        case pspgu::SceGeCommand::PRIM: {
            const uint64_t mesh_uid =
                static_cast<uint64_t>(command) | (static_cast<uint64_t>(ge_state_.vtx_buffer_addr) << 32);
            const auto iter = mesh_cache_.find(mesh_uid);

            // this mesh is not initialized yet
            if (mesh_cache_.end() == iter) {
                const auto num_vertices = command & 0x0000ffff;
                const auto primitive_type_raw = uint32_t{(command >> 16) & 0b0111};

                const auto primitive_type = (primitive_type_raw < eGxxPrimitiveCount)
                                                ? static_cast<GxxPrimitiveType>(primitive_type_raw)
                                                : eGxxPrimitiveTriangleStrip;

                if (ge_state_.vtx_buffer_addr == 0u) {
                    throw GxxParseError{"no vertex buffer pointer specifier"};
                }

                GxxMesh mesh;
                mesh.id = mesh_uid;
                mesh.num_weights = ge_state_.num_weights;
                mesh.primitive_type = primitive_type;

                if (ge_state_.idx_buffer_addr == 0) {
                    mesh.indices.reserve(num_vertices);

                    for (uint32_t vert_idx = 0; vert_idx < num_vertices; ++vert_idx) {
                        mesh.indices.emplace_back(vert_idx);
                    }
                }

                AssertSeek(reader, ge_state_.vtx_buffer_addr);
                const auto begin_offset = reader.Position();

                mesh.vertices = ReadAlignedVertexBuffer(reader, num_vertices);
                mesh.ge_buffer_size = static_cast<uint32_t>(reader.Position() - begin_offset);

                ge_state_.vtx_buffer_addr += mesh.ge_buffer_size;

                gxx_model.meshes.emplace_back(std::move(mesh));

                const auto mesh_id = static_cast<uint32_t>(gxx_model.meshes.size() - 1);

                mesh_cache_.emplace(std::make_pair(mesh_uid, mesh_id));
                ge_state_.mesh.emplace(mesh_id);
            } else {
                ge_state_.mesh.emplace(iter->second);
                ge_state_.vtx_buffer_addr += gxx_model.meshes[iter->second].ge_buffer_size;
            }

            ge_state_.mtx_unused_flag = false;

            // build a drawlist
            GxxDrawlist drawlist;
            drawlist.mesh = ge_state_.mesh;
            drawlist.albedo_color = ge_state_.albedo_color;

            if (ge_state_.enable_texture_map) {
                if (ge_state_.texture.has_value()) {
                    drawlist.texture = ge_state_.texture.value();
                } else {
                    logger_.log(fmt::format("warning: anim frame enables texture map but sets none"));
                }
            }

            if (ge_state_.num_weights > 0) {
                drawlist.bone_matrices = ge_state_.bone_matrix;
                drawlist.num_weights = ge_state_.num_weights;
            } else {
                drawlist.num_weights = 0;
            }

            drawlist.world_matrix = ge_state_.world_matrix;
            drawlist.uv_offset = ge_state_.uv_offset;
            drawlist.uv_scale = ge_state_.uv_scale;
            frame.list.emplace_back(std::move(drawlist));

            break;
        }

        case pspgu::SceGeCommand::NOP:

        // ppsspp calls this NOP_FF, for whatever reason pspsdk called this
        // matrix normalize to match the enum name remains unchanged, but
        // handling shall simply ignore this as a "nop" anyways
        case 0xff: {
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

    auto ReadAlignedVertexBuffer(util::bytes::BinaryReader &reader, uint32_t num_vertices) -> std::vector<GxxVertex> {
        constexpr std::array<uint32_t, 8> kAligns = {0, 0, 1, 3, 1, 1, 1, 3};
        logger_.log(
            fmt::format(
                "load vertex array:\n"
                "\tfmt_texture:\t\t{}\n"
                "\tfmt_color:\t\t{}\n"
                "\tfmt_normal:\t\t{}\n"
                "\tfmt_vertex:\t\t{}\n"
                "\tfmt_weight:\t\t{}\n"
                "\tfmt_index:\t\t{}\n"
                "\tnum_weights:\t\t{}\n"
                "\tnum_morphs:\t\t{}",
                pspgu::ToString(ge_state_.format_texture), pspgu::ToString(ge_state_.format_color),
                pspgu::ToString(ge_state_.format_normal), pspgu::ToString(ge_state_.format_position),
                pspgu::ToString(ge_state_.format_weight), pspgu::ToString(ge_state_.format_index),
                ge_state_.num_weights, ge_state_.num_morphs));

        using Reader = util::bytes::BinaryReader;
        const auto fnReadUv = [&]() -> glm::fvec2 (*)(Reader &) {
            switch (ge_state_.format_texture) {
            case pspgu::eSceGuFmtTextureNONE:
                return []([[maybe_unused]] Reader &r) { return glm::fvec2{0.0f, 0.0f}; };
            case pspgu::eSceGuFmtTextureUBYTE:
                return [](Reader &r) { return AssertReadAlignedFVecN<uint8_t, 2>(r); };
            case pspgu::eSceGuFmtTextureUSHORT:
                return [](Reader &r) { return AssertReadAlignedFVecN<uint16_t, 2>(r); };
            case pspgu::eSceGuFmtTextureFLOAT:
                return [](Reader &r) { return AssertReadAlignedFVecN<2>(r); };
            default:
                throw GxxParseError{
                    fmt::format("unsupported texture uv format {}", static_cast<uint32_t>(ge_state_.format_texture))};
            }
        }();

        const auto fnReadColor = [&]() -> glm::fvec4 (*)(Reader &) {
            switch (ge_state_.format_color) {
            case pspgu::eSceGuFmtColorNONE:
                return []([[maybe_unused]] Reader &r) { return glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f}; };
            case pspgu::eSceGuFmtColorPF5650:
                return [](Reader &r) {
                    const auto rgba = AssertReadAligned<uint16_t>(r);
                    return pspgu::ColorPF5650ToRGBA8(rgba);
                };
            case pspgu::eSceGuFmtColorPF5551:
                return [](Reader &r) {
                    const auto rgba = AssertReadAligned<uint16_t>(r);
                    return pspgu::ColorPF5551ToRGBA8(rgba);
                };
            case pspgu::eSceGuFmtColorPF4444:
                return [](Reader &r) {
                    const auto rgba = AssertReadAligned<uint16_t>(r);
                    return pspgu::ColorPF4444ToRGBA8(rgba);
                };
            case pspgu::eSceGuFmtColorPF8888:
                return [](Reader &r) {
                    const auto rgba = AssertReadAligned<uint32_t>(r);
                    return pspgu::ColorPF8888ToRGBA8(rgba);
                };
            default:
                throw GxxParseError{
                    fmt::format("unsupported vertex color format {}", static_cast<uint32_t>(ge_state_.format_color))};
            }
        }();

        const auto fnReadNormal = [&]() -> glm::fvec3 (*)(Reader &) {
            switch (ge_state_.format_normal) {
            case pspgu::eSceGuFmtNormalNONE:
                return []([[maybe_unused]] Reader &r) { return glm::fvec3{0.0f, 0.0f, 0.0f}; };
            case pspgu::eSceGuFmtNormalBYTE:
                return [](Reader &r) { return AssertReadAlignedFVecN<int8_t, 3>(r); };
            case pspgu::eSceGuFmtNormalSHORT:
                return [](Reader &r) { return AssertReadAlignedFVecN<int16_t, 3>(r); };
            case pspgu::eSceGuFmtNormalFLOAT:
                return [](Reader &r) { return AssertReadAlignedFVecN<3>(r); };
            default:
                throw GxxParseError{
                    fmt::format("unsupported vertex normal format {}", static_cast<uint32_t>(ge_state_.format_normal))};
            }
        }();

        const auto fnReadPosition = [&]() -> glm::fvec3 (*)(Reader &) {
            switch (ge_state_.format_position) {
            case pspgu::eSceGuFmtVertexNONE:
                return []([[maybe_unused]] Reader &r) { return glm::fvec3{0.0f, 0.0f, 0.0f}; };
            case pspgu::eSceGuFmtVertexBYTE:
                return [](Reader &r) { return AssertReadAlignedFVecN<int8_t, 3>(r); };
            case pspgu::eSceGuFmtVertexSHORT:
                return [](Reader &r) { return AssertReadAlignedFVecN<int16_t, 3>(r); };
            case pspgu::eSceGuFmtVertexFLOAT:
                return [](Reader &r) { return AssertReadAlignedFVecN<3>(r); };
            default:
                throw GxxParseError{fmt::format(
                    "unsupported vertex position format {}", static_cast<uint32_t>(ge_state_.format_position))};
            }
        }();

        const auto fnReadWeight = [&]() -> float (*)(Reader &) {
            switch (ge_state_.format_weight) {
            case pspgu::eSceGuFmtWeightNONE:
                return []([[maybe_unused]] Reader &r) { return 0.0f; };
            case pspgu::eSceGuFmtWeightUBYTE:
                return []([[maybe_unused]] Reader &r) {
                    return static_cast<float>(AssertReadAligned<uint8_t>(r)) / 255.0f;
                };
            case pspgu::eSceGuFmtWeightUSHORT:
                return []([[maybe_unused]] Reader &r) {
                    return static_cast<float>(AssertReadAligned<uint16_t>(r)) / 65535.0f;
                };
            case pspgu::eSceGuFmtWeightFLOAT:
                return []([[maybe_unused]] Reader &r) { return AssertReadAligned<float>(r); };
            default:
                throw GxxParseError{
                    fmt::format("unsupported vertex weight format {}", static_cast<uint32_t>(ge_state_.format_weight))};
            }
        }();

        const uint32_t align = kAligns[ge_state_.format_texture] | kAligns[ge_state_.format_color] |
                               kAligns[ge_state_.format_normal] | kAligns[ge_state_.format_position] |
                               kAligns[ge_state_.format_weight];

        std::vector<GxxVertex> vertices;
        vertices.reserve(num_vertices);
        for (uint32_t i = 0; i < num_vertices; ++i) {
            GxxVertex vertex;

            for (uint32_t w = 0; w < ge_state_.num_weights; ++w) {
                vertex.weights[w] = fnReadWeight(reader);
            }

            vertex.uv = fnReadUv(reader);
            vertex.color = fnReadColor(reader);
            vertex.normal = fnReadNormal(reader);
            vertex.position = fnReadPosition(reader);

            vertices.emplace_back(vertex);

            const auto position = reader.Position();
            const auto aligned_position = (position + align) & ~align;
            if (aligned_position > position) {
                (void)reader.ReadBuffer(aligned_position - position);
            }
        }

        return vertices;
    }

    const GxxLogger &logger_;

    std::span<const uint8_t> buffer_;
    std::map<uint64_t, uint32_t> mesh_cache_;
    std::map<uint32_t, uint32_t> texture_cache_;

    // ge state
    struct GEState {
        pspgu::SceGuFmtColor format_color = pspgu::eSceGuFmtColorNONE;
        pspgu::SceGuFmtNormal format_normal = pspgu::eSceGuFmtNormalNONE;
        pspgu::SceGuFmtTexture format_texture = pspgu::eSceGuFmtTextureNONE;
        pspgu::SceGuFmtVertex format_position = pspgu::eSceGuFmtVertexNONE;
        pspgu::SceGuFmtWeight format_weight = pspgu::eSceGuFmtWeightNONE;
        pspgu::SceGuFmtIndex format_index = pspgu::eSceGuFmtIndexNONE;
        uint32_t num_weights = 0;
        uint32_t num_morphs = 0;

        glm::fmat4x4 world_matrix = glm::fmat4x4{1.0f};
        bool mtx_unused_flag = false;

        std::array<glm::fmat4x4, 8> bone_matrix = {
            glm::fmat4x4{1.0f}, glm::fmat4x4{1.0f}, glm::fmat4x4{1.0f}, glm::fmat4x4{1.0f},
            glm::fmat4x4{1.0f}, glm::fmat4x4{1.0f}, glm::fmat4x4{1.0f}, glm::fmat4x4{1.0f},
        };

        std::optional<uint32_t> mesh = std::nullopt;
        std::optional<uint32_t> texture = std::nullopt;

        uint32_t vtx_buffer_addr = 0;
        uint32_t idx_buffer_addr = 0;

        bool enable_texture_map = false;

        glm::fvec2 uv_scale = glm::fvec2{1.0f, 1.0f};
        glm::fvec2 uv_offset = glm::fvec2{0.0f, 0.0f};

        glm::fvec4 albedo_color = glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f};

        std::stack<uint32_t> callstack;
        uint32_t program_counter;

        std::map<uint64_t, uint32_t> mesh_cache_;
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