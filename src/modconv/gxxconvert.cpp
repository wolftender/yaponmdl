#include <algorithm>
#include <iterator>
#include <vector>

#include <glm/gtx/matrix_decompose.hpp>

#include "modconv/modelconvert.hpp"

namespace {

class ConvertConsoleLogger final : public conv::IConvertLogger {
public:
    auto Log(std::string_view log_message) const -> void override {
        fmt::println("[libconv console log] {}", log_message);
    }
};

auto MakeErrorBitmap() -> const conv::ITextureRepository::Bitmap {
    conv::ITextureRepository::Bitmap bitmap;
    bitmap.width = 4;
    bitmap.height = 4;
    bitmap.uv_scale = glm::fvec2{1.0f, 1.0f};
    bitmap.uv_offset = glm::fvec2{0.0f, 0.0f};

    bitmap.plane.resize(bitmap.width * bitmap.height * 4);
    for (uint32_t i = 0; i < bitmap.width * bitmap.height; ++i) {
        const auto x = i % bitmap.width;
        const auto y = i / bitmap.width;
        const auto gx = x / 2;
        const auto gy = y / 2;
        const uint8_t v = 255 * ((gx + gy) % 2);

        bitmap.plane[4 * i + 0] = v;
        bitmap.plane[4 * i + 1] = 0;
        bitmap.plane[4 * i + 2] = v;
        bitmap.plane[4 * i + 3] = 255;
    }

    return bitmap;
}

auto FetchBitmap(const conv::ITextureRepository *repository, std::string_view name)
    -> conv::ITextureRepository::Bitmap {
    if (!repository) {
        return MakeErrorBitmap();
    }

    const auto bm = repository->FetchTexture(name);
    if (!bm.has_value()) {
        return MakeErrorBitmap();
    }

    return bm.value();
}

} // namespace

namespace conv {

auto ConvertIndicesGXX(const std::vector<uint32_t> &gxx_indices, gxx::GxxPrimitiveType primitive)
    -> std::vector<uint32_t> {
    std::vector<uint32_t> indices;
    switch (primitive) {
    case gxx::GxxPrimitiveType::eGxxPrimitiveTriangles:
        return gxx_indices;

    case gxx::GxxPrimitiveType::eGxxPrimitiveTriangleFan:
        for (uint32_t i = 1; i < gxx_indices.size() - 1; ++i) {
            indices.emplace_back(gxx_indices[0]);
            indices.emplace_back(gxx_indices[i + 0]);
            indices.emplace_back(gxx_indices[i + 1]);
        }

        break;

    case gxx::GxxPrimitiveType::eGxxPrimitiveTriangleStrip:
        for (uint32_t c = 0; c < gxx_indices.size() - 2; ++c) {
            if (c % 2 == 0) {
                indices.emplace_back(gxx_indices[c + 0]);
                indices.emplace_back(gxx_indices[c + 1]);
                indices.emplace_back(gxx_indices[c + 2]);
            } else {
                indices.emplace_back(gxx_indices[c + 0]);
                indices.emplace_back(gxx_indices[c + 2]);
                indices.emplace_back(gxx_indices[c + 1]);
            }
        }

        break;

    default:
        throw std::runtime_error{"unsupported primitive type"};
    }

    return indices;
}

template <render::Model::Animation::NodeTargetProperty Prop>
auto ChannelBuilderGXX(
    const render::Model::NodeId target,
    const std::vector<typename render::Model::Animation::NodeChannel<Prop>::PropertyType> &data, float fps,
    render::Model::Animation &animation) -> void {
    using KeyframeType = typename render::Model::Animation::NodeChannel<Prop>::KeyframeType;
    const float delta_time = 1.0f / fps;

    animation.AppendNodeChannel<Prop>(target, [&](render::Model::Animation::NodeChannel<Prop> &channel) -> void {
        switch (Prop) {
        case render::Model::Animation::NodeTargetProperty::eTranslation:
        case render::Model::Animation::NodeTargetProperty::eScale:
        case render::Model::Animation::NodeTargetProperty::eRotation:
            if (data.size() >= 2) {
                channel.SetInterpolation(render::Model::Animation::InterpolationMode::eLinear);
            } else {
                channel.SetInterpolation(render::Model::Animation::InterpolationMode::eStep);
            }

            break;

        default:
            channel.SetInterpolation(render::Model::Animation::InterpolationMode::eStep);
            break;
        }

        for (uint32_t i = 0; i < data.size(); ++i) {
            channel.GetKeyframes().emplace_back(
                KeyframeType{.value = data[i], .time = delta_time * static_cast<float>(i)});
        }
    });
}

auto ConvertGXX(
    const gxx::GxxModel &gxx_model, render::hal::IDevice *device, const ITextureRepository *repository,
    const IConvertLogger *logger) -> std::unique_ptr<render::Model> {
    ConvertConsoleLogger console_logger;
    if (!logger) {
        logger = &console_logger;
    }

    using AnyMeshId = std::variant<render::Model::MeshId, render::Model::AnimatedMeshId, std::monostate>;

    struct MeshData {
        AnyMeshId mesh_id;
        render::Model::NodeId root_bone;
        std::array<std::optional<render::Model::NodeId>, 8> deform_bones;
        uint32_t num_deform_bones;
    };

    auto model = std::make_unique<render::Model>(device);
    std::vector<render::Model::NodeId> bone_map;
    std::vector<render::Model::MaterialId> material_map;
    std::vector<MeshData> mesh_map;

    render::Model::Material::Description material_desc = {
        .diffuse = std::nullopt,
        .normal = std::nullopt,
        .specular = std::nullopt,
        .color = glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f},
    };

    const auto fallback_mat_id = model->AddMaterial(material_desc);
    if (!fallback_mat_id.has_value()) {
        throw std::runtime_error{"failed to allocate fallback material"};
    }

    render::Model::MaterialId fallback_material = fallback_mat_id.value();

    for (const auto &gxx_node : gxx_model.nodes) {
        const auto bone_id = model->AddNode(model->GetRoot(), gxx_node.name);
        if (!bone_id.has_value()) {
            throw std::runtime_error{"failed to allocate model node"};
        }

        bone_map.push_back(bone_id.value());
    }

    for (const auto &gxx_texture : gxx_model.textures) {
        const auto bitmap = FetchBitmap(repository, gxx_texture.name);
        const auto texture_id = model->AddRgbaTexture(bitmap.width, bitmap.height, bitmap.plane);

        render::Model::Material::Description material_desc = {
            .diffuse = texture_id,
            .normal = std::nullopt,
            .specular = std::nullopt,
            .uv_offset = bitmap.uv_offset,
            .uv_scale = bitmap.uv_scale,
        };

        const auto material_id = model->AddMaterial(material_desc);
        if (!material_id.has_value()) {
            throw std::runtime_error{"failed to allocate model material"};
        }

        material_map.push_back(material_id.value());
    }

    for (uint32_t gxx_mesh_id = 0; gxx_mesh_id < gxx_model.meshes.size(); ++gxx_mesh_id) {
        const auto &gxx_mesh = gxx_model.meshes[gxx_mesh_id];

        // since gxx is a drawlist based format, it can freely match any texture with a model
        // here, we hope that the first used texture is also the only used texture
        // until material animations are introduced to the scene graph
        std::optional<uint32_t> first_used_texture = std::nullopt;

        for (const auto &gxx_animation : gxx_model.animations) {
            for (const auto &gxx_frame : gxx_animation.frames) {
                for (const auto &gxx_drawlist : gxx_frame.list) {
                    if (!gxx_drawlist.mesh.has_value()) {
                        continue;
                    }

                    if (gxx_drawlist.mesh.value() != gxx_mesh_id) {
                        continue;
                    }

                    if (!gxx_drawlist.texture.has_value()) {
                        continue;
                    }

                    first_used_texture = gxx_drawlist.texture.value();
                    break;
                }
            }
        }

        const render::Model::MaterialId material = [&]() {
            if (first_used_texture.has_value()) {
                if (first_used_texture.value() < material_map.size()) {
                    return material_map[first_used_texture.value()];
                } else {
                    logger->Log(
                        fmt::format(
                            "warning: invalid texture reference outsie of bounds {}", first_used_texture.value()));
                }
            }

            return fallback_material;
        }();

        if (gxx_mesh.num_weights == 0) {
            std::vector<render::StaticVertex> vertices;
            vertices.reserve(gxx_mesh.vertices.size());

            std::transform(
                gxx_mesh.vertices.begin(), gxx_mesh.vertices.end(), std::back_inserter(vertices),
                [&](const gxx::GxxVertex &gxx_vertex) {
                render::StaticVertex vertex;
                vertex.position = gxx_vertex.position;
                vertex.color = gxx_vertex.color;
                vertex.normal = gxx_vertex.normal;
                vertex.uv = gxx_vertex.uv;

                return vertex;
            });

            const auto mesh_id =
                gxx_mesh.primitive_type == gxx::eGxxPrimitiveTriangles
                    ? model->AddMesh(vertices, gxx_mesh.indices, material)
                    : model->AddMesh(vertices, ConvertIndicesGXX(gxx_mesh.indices, gxx_mesh.primitive_type), material);
            if (!mesh_id.has_value()) {
                throw std::runtime_error{"failed to allocate model mesh"};
            }

            const auto mesh_node_id = model->AddNode(model->GetRoot(), fmt::format("gxx_mesh_{}", gxx_mesh_id));
            if (!mesh_node_id.has_value()) {
                throw std::runtime_error{"failed to allocate model mesh node"};
            }

            auto *mesh_node = model->GetNode(mesh_node_id.value());
            if (mesh_node) {
                mesh_node->AddMesh(mesh_id.value());
            }

            MeshData mesh_data{
                .mesh_id = mesh_id.value(),
                .root_bone = mesh_node_id.value(),
                .deform_bones = {},
                .num_deform_bones = 0,
            };

            mesh_map.emplace_back(mesh_data);
        } else {
            std::vector<render::AnimatedVertex> vertices;
            vertices.reserve(gxx_mesh.vertices.size());

            std::transform(
                gxx_mesh.vertices.begin(), gxx_mesh.vertices.end(), std::back_inserter(vertices),
                [&](const gxx::GxxVertex &gxx_vertex) {
                render::StaticVertex vertex;
                vertex.position = gxx_vertex.position;
                vertex.color = gxx_vertex.color;
                vertex.normal = gxx_vertex.normal;
                vertex.uv = gxx_vertex.uv;

                for (uint32_t i = 0; i < gxx_mesh.num_weights; ++i) {
                    vertex.weights[i] = gxx_vertex.weights[i];
                    vertex.bones[i] = i;
                }

                return vertex;
            });

            std::array<std::optional<render::Model::NodeId>, 8> deform_nodes;

            // since this is a skinned mesh, we add some bones parented to the root
            render::Model::Skin skin;
            for (uint32_t i = 0; i < gxx_mesh.num_weights; ++i) {
                const auto deform_bone_id =
                    model->AddNode(model->GetRoot(), fmt::format("m{:x}_deform{}", gxx_mesh.id, i));
                if (!deform_bone_id) {
                    throw std::runtime_error{"failed to allocate deform bone for mesh"};
                }

                skin.AddNodeRef(deform_bone_id.value(), glm::fmat4x4{1.0f});
                deform_nodes[i] = deform_bone_id.value();
            }

            const auto skin_id = model->AddSkin(std::move(skin));
            if (!skin_id.has_value()) {
                throw std::runtime_error{"failed to allocate deform skin for mesh"};
            }

            const auto mesh_id = gxx_mesh.primitive_type == gxx::eGxxPrimitiveTriangles
                                     ? model->AddAnimatedMesh(vertices, gxx_mesh.indices, material, skin_id.value())
                                     : model->AddAnimatedMesh(
                                           vertices, ConvertIndicesGXX(gxx_mesh.indices, gxx_mesh.primitive_type),
                                           material, skin_id.value());
            if (!mesh_id.has_value()) {
                throw std::runtime_error{"failed to allocate model animated mesh"};
            }

            const auto mesh_node_id = model->AddNode(model->GetRoot(), fmt::format("gxx_mesh_{}", gxx_mesh_id));
            if (!mesh_node_id.has_value()) {
                throw std::runtime_error{"failed to allocate model mesh node"};
            }

            auto *mesh_node = model->GetNode(mesh_node_id.value());
            if (mesh_node) {
                mesh_node->AddAnimMesh(mesh_id.value());
            }

            MeshData mesh_data = {
                .mesh_id = mesh_id.value(),
                .root_bone = mesh_node_id.value(),
                .deform_bones = deform_nodes,
                .num_deform_bones = gxx_mesh.num_weights,
            };

            mesh_map.emplace_back(mesh_data);
        }
    }

    using NodeTargetProperty = render::Model::Animation::NodeTargetProperty;

    for (const auto &gxx_animation : gxx_model.animations) {
        render::Model::Animation animation{gxx_animation.name};

        // really annoying part to do
        // we need to map out the motions from a file that basically contains drawlists
        // this probably should be changed to have its OWN model class in the future
        std::vector<glm::fvec3> translation_keys;
        std::vector<glm::fvec3> scale_keys;
        std::vector<glm::fquat> rotation_keys;
        std::vector<glm::fvec2> uv_offs_keys;
        std::vector<glm::fvec2> uv_scale_keys;
        std::vector<glm::fvec4> color_keys;

        translation_keys.resize(gxx_animation.num_frames);
        scale_keys.resize(gxx_animation.num_frames);
        rotation_keys.resize(gxx_animation.num_frames);
        uv_offs_keys.resize(gxx_animation.num_frames);
        uv_scale_keys.resize(gxx_animation.num_frames);
        color_keys.resize(gxx_animation.num_frames);

        struct DeformBoneFrames {
            std::vector<glm::fvec3> translation_keys;
            std::vector<glm::fvec3> scale_keys;
            std::vector<glm::fquat> rotation_keys;
            std::optional<render::Model::NodeId> node;
            bool used;
        };

        std::array<DeformBoneFrames, 8> deform_bone_keys;

        for (uint32_t mesh_idx = 0; mesh_idx < mesh_map.size(); ++mesh_idx) {
            const auto &mesh_data = mesh_map[mesh_idx];
            for (uint32_t i = 0; i < 8; ++i) {
                if (mesh_data.deform_bones[i].has_value()) {
                    deform_bone_keys[i].translation_keys.resize(gxx_animation.num_frames);
                    deform_bone_keys[i].rotation_keys.resize(gxx_animation.num_frames);
                    deform_bone_keys[i].scale_keys.resize(gxx_animation.num_frames);
                    deform_bone_keys[i].node = mesh_data.deform_bones[i];
                    deform_bone_keys[i].used = true;
                } else {
                    deform_bone_keys[i].used = false;
                }
            }

            for (uint32_t gxx_frame_idx = 0; gxx_frame_idx < gxx_animation.num_frames; ++gxx_frame_idx) {
                const auto &gxx_frame = gxx_animation.frames[gxx_frame_idx];

                const auto count_reuses = std::count_if(
                    gxx_frame.list.begin(), gxx_frame.list.end(), [&](const gxx::GxxDrawlist &list) -> bool {
                    return list.mesh.has_value() && list.mesh.value() == mesh_idx;
                });

                if (count_reuses > 1) {
                    logger->Log(fmt::format("SAME MESH RESUED IN FRAE {} TIMES", count_reuses));
                }

                // find the display list for current mesh (if any)
                const auto display_list_itr = std::find_if(
                    gxx_frame.list.begin(), gxx_frame.list.end(), [&](const gxx::GxxDrawlist &list) -> bool {
                    return list.mesh.has_value() && list.mesh.value() == mesh_idx;
                });

                if (display_list_itr == gxx_frame.list.end()) {
                    translation_keys[gxx_frame_idx] = glm::fvec3{0.0f};
                    scale_keys[gxx_frame_idx] = glm::fvec3{0.0f};
                    rotation_keys[gxx_frame_idx] = glm::fvec3{0.0f};
                    uv_offs_keys[gxx_frame_idx] = glm::fvec2{0.0f};
                    uv_scale_keys[gxx_frame_idx] = glm::fvec2{0.0f};
                    color_keys[gxx_frame_idx] = glm::fvec4{1.0f};

                    if (mesh_data.num_deform_bones > 0) {
                        for (uint32_t i = 0; i < 8; ++i) {
                            if (mesh_data.deform_bones[i].has_value()) {
                                deform_bone_keys[i].translation_keys[gxx_frame_idx] = glm::fvec3{0.0f};
                                deform_bone_keys[i].rotation_keys[gxx_frame_idx] = glm::fvec3{0.0f};
                                deform_bone_keys[i].scale_keys[gxx_frame_idx] = glm::fvec3{0.0f};
                            }
                        }
                    }
                } else {
                    // decompose matrix
                    [[maybe_unused]] glm::fvec3 skew;
                    [[maybe_unused]] glm::fvec4 perspective;
                    glm::decompose(
                        display_list_itr->world_matrix, scale_keys[gxx_frame_idx], rotation_keys[gxx_frame_idx],
                        translation_keys[gxx_frame_idx], skew, perspective);

                    uv_offs_keys[gxx_frame_idx] = display_list_itr->uv_offset;
                    uv_scale_keys[gxx_frame_idx] = display_list_itr->uv_scale;
                    color_keys[gxx_frame_idx] = display_list_itr->albedo_color;

                    for (uint32_t i = 0; i < 8; ++i) {
                        if (deform_bone_keys[i].used) {
                            glm::decompose(
                                display_list_itr->bone_matrices[i], deform_bone_keys[i].scale_keys[gxx_frame_idx],
                                deform_bone_keys[i].rotation_keys[gxx_frame_idx],
                                deform_bone_keys[i].translation_keys[gxx_frame_idx], skew, perspective);
                        }
                    }
                }
            }

            for (uint32_t i = 0; i < 8; ++i) {
                if (deform_bone_keys[i].used && deform_bone_keys[i].node.has_value()) {
                    ChannelBuilderGXX<NodeTargetProperty::eTranslation>(
                        deform_bone_keys[i].node.value(), deform_bone_keys[i].translation_keys, gxx_animation.framerate,
                        animation);
                    ChannelBuilderGXX<NodeTargetProperty::eScale>(
                        deform_bone_keys[i].node.value(), deform_bone_keys[i].scale_keys, gxx_animation.framerate,
                        animation);
                    ChannelBuilderGXX<NodeTargetProperty::eRotation>(
                        deform_bone_keys[i].node.value(), deform_bone_keys[i].rotation_keys, gxx_animation.framerate,
                        animation);
                }
            }

            ChannelBuilderGXX<NodeTargetProperty::eTranslation>(
                mesh_data.root_bone, translation_keys, gxx_animation.framerate, animation);
            ChannelBuilderGXX<NodeTargetProperty::eScale>(
                mesh_data.root_bone, scale_keys, gxx_animation.framerate, animation);
            ChannelBuilderGXX<NodeTargetProperty::eRotation>(
                mesh_data.root_bone, rotation_keys, gxx_animation.framerate, animation);
            ChannelBuilderGXX<NodeTargetProperty::eUvOffset>(
                mesh_data.root_bone, uv_offs_keys, gxx_animation.framerate, animation);
            ChannelBuilderGXX<NodeTargetProperty::eUvScale>(
                mesh_data.root_bone, uv_scale_keys, gxx_animation.framerate, animation);
            ChannelBuilderGXX<NodeTargetProperty::eColor>(
                mesh_data.root_bone, color_keys, gxx_animation.framerate, animation);
        }

        model->AddAnimation(std::move(animation));
    }

    return model;
}

auto ConvertGXXDrawlist(
    const gxx::GxxModel &gxx_model, render::hal::IDevice *device, const ITextureRepository *repository,
    const IConvertLogger *logger) -> std::unique_ptr<render::Drawlist> {
    ConvertConsoleLogger console_logger;
    if (!logger) {
        logger = &console_logger;
    }

    using AnyMeshId =
        std::variant<render::Drawlist::VertexBufferId, render::Drawlist::SkinnedVertexBufferId, std::monostate>;

    auto drawlist = std::make_unique<render::Drawlist>(device);

    std::vector<std::pair<render::Drawlist::NodeId, uint32_t>> bone_map;
    std::vector<render::Drawlist::TextureId> texture_map;
    std::vector<AnyMeshId> mesh_map;

    for (const auto &gxx_node : gxx_model.nodes) {
        const auto bone_id = drawlist->AddNamedNode(gxx_node.name);
        if (!bone_id.has_value()) {
            throw std::runtime_error{"failed to allocate drawlist node"};
        }

        bone_map.push_back({bone_id.value(), gxx_node.ge_matrix_offs});
    }

    for (const auto &gxx_texture : gxx_model.textures) {
        const auto bitmap = FetchBitmap(repository, gxx_texture.name);
        const auto texture_id = drawlist->AddRgbaTexture(bitmap.width, bitmap.height, bitmap.plane);

        if (!texture_id.has_value()) {
            throw std::runtime_error{"failed to allocate drawlist texture"};
        }

        texture_map.push_back(texture_id.value());
    }

    for (uint32_t gxx_mesh_id = 0; gxx_mesh_id < gxx_model.meshes.size(); ++gxx_mesh_id) {
        const auto &gxx_mesh = gxx_model.meshes[gxx_mesh_id];
        if (gxx_mesh.num_weights == 0) {
            std::vector<render::StaticVertex> vertices;
            vertices.reserve(gxx_mesh.vertices.size());

            std::transform(
                gxx_mesh.vertices.begin(), gxx_mesh.vertices.end(), std::back_inserter(vertices),
                [&](const gxx::GxxVertex &gxx_vertex) {
                render::StaticVertex vertex;
                vertex.position = gxx_vertex.position;
                vertex.color = gxx_vertex.color;
                vertex.normal = gxx_vertex.normal;
                vertex.uv = gxx_vertex.uv;

                return vertex;
            });

            const auto vertex_buffer_id =
                gxx_mesh.primitive_type == gxx::eGxxPrimitiveTriangles
                    ? drawlist->AddVertexBuffer(vertices, gxx_mesh.indices)
                    : drawlist->AddVertexBuffer(vertices, ConvertIndicesGXX(gxx_mesh.indices, gxx_mesh.primitive_type));
            if (!vertex_buffer_id.has_value()) {
                throw std::runtime_error{"failed to allocate drawlist vertex buffer"};
            }

            mesh_map.emplace_back(vertex_buffer_id.value());
        } else {
            std::vector<render::AnimatedVertex> vertices;
            vertices.reserve(gxx_mesh.vertices.size());

            std::transform(
                gxx_mesh.vertices.begin(), gxx_mesh.vertices.end(), std::back_inserter(vertices),
                [&](const gxx::GxxVertex &gxx_vertex) {
                render::StaticVertex vertex;
                vertex.position = gxx_vertex.position;
                vertex.color = gxx_vertex.color;
                vertex.normal = gxx_vertex.normal;
                vertex.uv = gxx_vertex.uv;

                for (uint32_t i = 0; i < gxx_mesh.num_weights; ++i) {
                    vertex.weights[i] = gxx_vertex.weights[i];
                    vertex.bones[i] = i;
                }

                return vertex;
            });

            const auto vertex_buffer_id =
                gxx_mesh.primitive_type == gxx::eGxxPrimitiveTriangles
                    ? drawlist->AddSkinnedVertexBuffer(vertices, gxx_mesh.indices)
                    : drawlist->AddSkinnedVertexBuffer(
                          vertices, ConvertIndicesGXX(gxx_mesh.indices, gxx_mesh.primitive_type));
            if (!vertex_buffer_id.has_value()) {
                throw std::runtime_error{"failed to allocate drawlist vertex buffer"};
            }

            mesh_map.emplace_back(vertex_buffer_id.value());
        }
    }

    for (const auto &gxx_motion : gxx_model.animations) {
        drawlist->AddMotion(gxx_motion.name, [&](render::Drawlist::Motion &motion) -> void {
            const float delta_time = 1.0f / gxx_motion.framerate;

            for (uint32_t i = 0; i < gxx_motion.num_frames; ++i) {
                const auto &gxx_frame = gxx_motion.frames[i];

                render::Drawlist::Motion::Frame frame{static_cast<float>(i) * delta_time};
                for (const auto &gxx_list : gxx_frame.list) {
                    if (!gxx_list.mesh.has_value()) {
                        continue;
                    }

                    std::optional<render::Drawlist::TextureId> diffuse_map = std::nullopt;
                    if (gxx_list.texture.has_value()) {
                        diffuse_map = texture_map[gxx_list.texture.value()];
                    }

                    if (gxx_list.num_weights == 0) {
                        // regular draw
                        if (!std::holds_alternative<render::Drawlist::VertexBufferId>(
                                mesh_map[gxx_list.mesh.value()])) {
                            throw std::runtime_error{fmt::format(
                                "gxx references {} mesh but this is not a valid unskinned mesh",
                                gxx_list.mesh.value())};
                        }

                        const auto mesh_id =
                            std::get<render::Drawlist::VertexBufferId>(mesh_map[gxx_list.mesh.value()]);

                        render::Drawlist::Motion::DrawCommand command = {
                            .mesh = mesh_id,
                            .parameters =
                                {
                                    .diffuse_map = diffuse_map,
                                    .uv_offset = gxx_list.uv_offset,
                                    .uv_scale = gxx_list.uv_scale,
                                    .color = gxx_list.albedo_color,
                                    .world_matix = gxx_list.world_matrix,
                                },
                        };

                        frame.AppendCommand(std::move(command));
                    } else {
                        // skinned draw
                        if (!std::holds_alternative<render::Drawlist::SkinnedVertexBufferId>(
                                mesh_map[gxx_list.mesh.value()])) {
                            throw std::runtime_error{fmt::format(
                                "gxx references {} mesh but this is not a valid skinned mesh", gxx_list.mesh.value())};
                        }

                        const auto mesh_id =
                            std::get<render::Drawlist::SkinnedVertexBufferId>(mesh_map[gxx_list.mesh.value()]);

                        const auto skin_id = drawlist->AddSkin(gxx_list.bone_matrices);
                        if (!skin_id.has_value()) {
                            throw std::runtime_error{"failed to allocate drawlist skin"};
                        }

                        render::Drawlist::Motion::SkinnedDrawCommand command = {
                            .mesh = mesh_id,
                            .skin = skin_id.value(),
                            .parameters =
                                {
                                    .diffuse_map = diffuse_map,
                                    .uv_offset = gxx_list.uv_offset,
                                    .uv_scale = gxx_list.uv_scale,
                                    .color = gxx_list.albedo_color,
                                    .world_matix = gxx_list.world_matrix,
                                },
                        };

                        frame.AppendCommand(std::move(command));
                    }
                }

                for (uint32_t gxx_matrix_id = 0; gxx_matrix_id < gxx_frame.node_matrices.size(); ++gxx_matrix_id) {
                    const auto named_node_id = bone_map[gxx_matrix_id].first;
                    frame.SetNodeOffset(named_node_id, gxx_frame.node_matrices[gxx_matrix_id]);
                }

                motion.AppendFrame(std::move(frame));
            }
        });
    }

    return drawlist;
}

} // namespace conv
