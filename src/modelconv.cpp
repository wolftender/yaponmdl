#include <numeric>
#include <queue>
#include <variant>

#include <fmt/format.h>
#include <stb_image/stb_image.h>

#include "modelconv.hpp"
#include "common/common.hpp"

namespace conv {

class ConvertConsoleLogger final : public IConvertLogger {
public:
    auto Log(std::string_view log_message) const -> void override {
        fmt::println("[libconv console log] {}", log_message);
    }
};

auto MakeErrorBitmap() -> const ITextureRepository::Bitmap {
    ITextureRepository::Bitmap bitmap;
    bitmap.width = 4;
    bitmap.height = 4;

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

auto FetchBitmap(const ITextureRepository *repository, std::string_view name) -> ITextureRepository::Bitmap {
    if (!repository) {
        return MakeErrorBitmap();
    }

    const auto bm = repository->FetchTexture(name);
    if (!bm.has_value()) {
        return MakeErrorBitmap();
    }

    return bm.value();
}

auto ConvertIndicesGMO(const gmo::GmoDrawArray &gmo_draw_array) -> std::vector<uint32_t> {
    std::vector<uint32_t> indices;
    switch (gmo_draw_array.primitive) {
    case gmo::GmoDrawPrimitive::eTriangles:
        for (uint32_t prim_idx = 0; prim_idx < gmo_draw_array.num_primitives; ++prim_idx) {
            uint32_t base = prim_idx * gmo_draw_array.num_vertices;
            std::copy(
                gmo_draw_array.indices.begin() + base,
                gmo_draw_array.indices.begin() + base + gmo_draw_array.num_vertices, std::back_inserter(indices));
        }

        break;

    case gmo::GmoDrawPrimitive::eTriangleFan:
        for (uint32_t prim_idx = 0; prim_idx < gmo_draw_array.num_primitives; ++prim_idx) {
            uint32_t base = prim_idx * gmo_draw_array.num_vertices;

            for (uint32_t i = 1; i < gmo_draw_array.num_vertices - 1; ++i) {
                indices.emplace_back(gmo_draw_array.indices[base + 0]);
                indices.emplace_back(gmo_draw_array.indices[base + i + 0]);
                indices.emplace_back(gmo_draw_array.indices[base + i + 1]);
            }
        }

        break;

    case gmo::GmoDrawPrimitive::eTriangleStrip:
        for (uint32_t prim_idx = 0; prim_idx < gmo_draw_array.num_primitives; ++prim_idx) {
            uint32_t base = prim_idx * gmo_draw_array.num_vertices;

            for (uint32_t c = 0; c < gmo_draw_array.num_vertices - 2; ++c) {
                if (c % 2 == 0) {
                    indices.emplace_back(gmo_draw_array.indices[base + c + 0]);
                    indices.emplace_back(gmo_draw_array.indices[base + c + 1]);
                    indices.emplace_back(gmo_draw_array.indices[base + c + 2]);
                } else {
                    indices.emplace_back(gmo_draw_array.indices[base + c + 0]);
                    indices.emplace_back(gmo_draw_array.indices[base + c + 2]);
                    indices.emplace_back(gmo_draw_array.indices[base + c + 1]);
                }
            }
        }

        break;

    default:
        throw std::runtime_error{"unsupported primitive type"};
    }

    return indices;
}

auto ConvertGMOInterpolation(gmo::GmoFCurveInterpolation interpolation) -> render::Model::Animation::InterpolationMode {
    switch (interpolation) {
    case gmo::GmoFCurveInterpolation::eConstant:
        return render::Model::Animation::InterpolationMode::eStep;
    case gmo::GmoFCurveInterpolation::eSpherical:
        return render::Model::Animation::InterpolationMode::eLinear;
    case gmo::GmoFCurveInterpolation::eLinear:
        return render::Model::Animation::InterpolationMode::eLinear;
    case gmo::GmoFCurveInterpolation::eHermite:
    case gmo::GmoFCurveInterpolation::eCubic:
        return render::Model::Animation::InterpolationMode::eStep;
    }
}

template <render::Model::Animation::TargetProperty Prop>
auto ConvertGMOKeyframes(
    render::Model::Animation::Channel<Prop> &channel, const gmo::GmoMotion &gmo_motion,
    const gmo::GmoAnimation &gmo_animation) -> void {
    using DataType = render::Model::Animation::Channel<Prop>::PropertyType;
    using KeyframeType = render::Model::Animation::Channel<Prop>::KeyframeType;

    constexpr uint32_t kNumDimensions = DataType::length();
    // constexpr std::array<uint32_t, 5> kElementsPerInterpType = {1, 1, 3, 5, 1};

    const auto &gmo_fcurve = gmo_motion.fcurves[gmo_animation.fcurve_id];
    const auto num_dims_per_keyframe = gmo::NumElementsPerInterpType(gmo_fcurve.interpolation) * kNumDimensions + 1;

    if (num_dims_per_keyframe * gmo_fcurve.num_keyframes > gmo_fcurve.raw_data.size()) {
        throw std::runtime_error{"invalid gmo fcurve, dimensions don't match"};
    }

    channel.SetInterpolation(ConvertGMOInterpolation(gmo_fcurve.interpolation));
    const auto fps = static_cast<float>(gmo_motion.framerate);

    for (uint32_t keyframe_idx = 0; keyframe_idx < gmo_fcurve.num_keyframes; ++keyframe_idx) {
        const auto base_idx = num_dims_per_keyframe * keyframe_idx;

        KeyframeType keyframe;
        keyframe.time = gmo_fcurve.raw_data[base_idx] / fps;

        for (uint32_t d = 0; d < kNumDimensions; ++d) {
            keyframe.value[d] = gmo_fcurve.raw_data[base_idx + 1 + d];
        }

        channel.GetKeyframes().emplace_back(std::move(keyframe));
    }
}

auto ConvertGMO(
    const gmo::GmoModel &gmo_model, render::hal::IDevice *device, const ITextureRepository *repository,
    const IConvertLogger *logger) -> std::unique_ptr<render::Model> {
    ConvertConsoleLogger console_logger;
    if (!logger) {
        logger = &console_logger;
    }

    auto model = std::make_unique<render::Model>(device);

    using AnyMeshId = std::variant<render::Model::MeshId, render::Model::AnimatedMeshId, std::monostate>;

    const auto num_bones = gmo_model.bones.size();
    const auto num_textures = gmo_model.textures.size();
    const auto num_materials = gmo_model.materials.size();
    const auto num_meshes =
        std::accumulate(gmo_model.parts.begin(), gmo_model.parts.end(), 0ull, [](uint64_t n, const auto &part) {
        return n + part.meshes.size();
    });

    logger->Log(
        fmt::format(
            "initializing mesh with:"
            "\n\tnum_bones: {}"
            "\n\tnum_textures: {}"
            "\n\tnum_materials: {}"
            "\n\tnum_meshes: {}",
            num_bones, num_textures, num_materials, num_meshes));

    std::vector<std::optional<render::Model::NodeId>> node_map;
    std::vector<std::optional<render::Model::TextureId>> texture_map;
    std::vector<std::optional<render::Model::MaterialId>> material_map;
    std::vector<AnyMeshId> mesh_map;

    node_map.resize(num_bones);
    texture_map.resize(num_textures);
    material_map.resize(num_materials);
    mesh_map.resize(num_meshes, std::monostate{});

    for (uint32_t gmo_texture_id = 0; gmo_texture_id < num_textures; ++gmo_texture_id) {
        const auto &gmo_texture = gmo_model.textures[gmo_texture_id];
        const auto bitmap = FetchBitmap(repository, gmo_texture.filename);
        const auto texture_id = model->AddRgbaTexture(bitmap.width, bitmap.height, bitmap.plane);

        texture_map[gmo_texture_id] = texture_id;
    }

    for (uint32_t gmo_material_id = 0; gmo_material_id < num_materials; ++gmo_material_id) {
        const auto &gmo_material = gmo_model.materials[gmo_material_id];
        render::Model::Material::Description desc;

        for (const auto &gmo_layer : gmo_material.layers) {
            switch (gmo_layer.map_type) {
            case gmo::GmoMaterialLayerMapType::eNone:
            case gmo::GmoMaterialLayerMapType::eDiffuse:
                logger->Log(
                    fmt::format(
                        "material {} has texture layer with diffuse texture {}", gmo_material.name,
                        gmo_layer.texture_id));
                desc.diffuse = texture_map[gmo_layer.texture_id];
                break;

            default:
                break;
            }
        }

        material_map[gmo_material_id] = model->AddMaterial(desc);
    }

    std::queue<uint32_t> node_queue;
    for (uint32_t gmo_bone_id = 0; gmo_bone_id < num_bones; ++gmo_bone_id) {
        node_queue.push(gmo_bone_id);
    }

    std::vector<uint32_t> nodes_with_meshes;
    for (uint32_t iter = 0; !node_queue.empty() && iter < num_bones * num_bones; ++iter) {
        auto gmo_bone_id = node_queue.front();
        node_queue.pop();

        const auto &gmo_bone = gmo_model.bones[gmo_bone_id];
        auto parent_id = model->GetRoot();

        if (gmo_bone.parent_bone.has_value()) {
            const auto gmo_parent_id = gmo_bone.parent_bone.value();
            if (!node_map[gmo_parent_id].has_value()) {
                node_queue.push(gmo_bone_id);
                continue;
            }

            parent_id = node_map[gmo_parent_id].value();
        }

        const auto node_id = model->AddNode(parent_id, gmo_bone.name).value();
        node_map[gmo_bone_id] = node_id;

        auto node = model->GetNode(node_id);

        node->SetTranslation(gmo_bone.translation);
        node->SetRotation(gmo_bone.rotation);
        node->SetScale(gmo_bone.scale);

        if (!gmo_bone.draw_parts.empty()) {
            nodes_with_meshes.push_back(gmo_bone_id);
        }
    }

    if (!node_queue.empty()) {
        throw std::runtime_error{"invalid model topology"};
    }

    // create bind pose to calculate bone offsets
    auto bind_pose = model->CreatePose();

    const auto fnCreateSkin = [&]([[maybe_unused]] const render::Model::Node &bone,
                                  const gmo::GmoBone &gmo_bone) -> std::optional<render::Model::SkinId> {
        if (gmo_bone.blend_bones.empty()) {
            return std::nullopt;
        }

        render::Model::Skin skin;
        for (uint32_t i = 0; i < gmo_bone.blend_bones.size(); ++i) {
            if (!node_map[gmo_bone.blend_bones[i]].has_value()) {
                throw std::runtime_error{"invalid relation between blend bones"};
            }

            const auto ref_bone_id = node_map[gmo_bone.blend_bones[i]].value();
            skin.AddNodeRef(ref_bone_id, gmo_bone.blend_offsets[i]);
        }

        return model->AddSkin(std::move(skin));
    };

    const auto fnParseGmoMesh = [&](render::Model::Node &node, const gmo::GmoMesh &gmo_mesh,
                                    const gmo::GmoDrawArray &gmo_draw_array,
                                    const gmo::GmoVertexArray &gmo_vertex_array) {
        std::vector<render::AnimatedVertex> vertices;
        vertices.reserve(gmo_vertex_array.vertices.size());

        std::transform(
            gmo_vertex_array.vertices.begin(), gmo_vertex_array.vertices.end(), std::back_inserter(vertices),
            [&](const gmo::GmoVertex &gmo_vert) {
            render::AnimatedVertex vertex;
            vertex.position = gmo_vert.position;
            vertex.normal = gmo_vert.normal;
            vertex.color = gmo_vert.color;
            vertex.uv = gmo_vert.uv;

            return vertex;
        });

        auto material_id = material_map[gmo_mesh.material];
        if (!material_id.has_value()) {
            throw std::runtime_error{"invalid material was referenced in the gmo model"};
        }

        const auto mesh_id = model->AddMesh(vertices, ConvertIndicesGMO(gmo_draw_array), material_id.value());
        if (!mesh_id.has_value()) {
            throw std::runtime_error{"failed to allocate mesh"};
        }

        node.AddMesh(mesh_id.value());
    };

    const auto fnParseGmoSkinmesh = [&](render::Model::Node &node, const gmo::GmoBone &gmo_bone,
                                        const gmo::GmoMesh &gmo_mesh, const gmo::GmoDrawArray &gmo_draw_array,
                                        const gmo::GmoVertexArray &gmo_vertex_array) {
        const auto skin_id = fnCreateSkin(node, gmo_bone);
        const auto *skin = model->GetSkin(skin_id.value());

        std::vector<render::AnimatedVertex> vertices;
        vertices.reserve(gmo_vertex_array.vertices.size());

        std::transform(
            gmo_vertex_array.vertices.begin(), gmo_vertex_array.vertices.end(), std::back_inserter(vertices),
            [&](const gmo::GmoVertex &gmo_vert) {
            render::AnimatedVertex vertex;
            vertex.position = gmo_vert.position;
            vertex.normal = gmo_vert.normal;
            vertex.color = gmo_vert.color;
            vertex.uv = gmo_vert.uv;

            std::fill(vertex.weights, vertex.weights + 8, 0.0f);
            std::fill(vertex.bones, vertex.bones + 8, 0);

            // if no blend subset, pick first n bones, otherwise use blend subset
            if (gmo_mesh.blend_subset.empty()) {
                for (uint32_t i = 0; i < 8 && i < skin->GetNodes().size(); ++i) {
                    vertex.weights[i] = gmo_vert.weights[i];
                    vertex.bones[i] = i;
                }
            } else {
                for (uint32_t i = 0; i < gmo_mesh.blend_subset.size(); ++i) {
                    vertex.weights[i] = gmo_vert.weights[i];
                    vertex.bones[i] = gmo_mesh.blend_subset[i];
                }
            }

            return vertex;
        });

        auto material_id = material_map[gmo_mesh.material];
        if (!material_id.has_value()) {
            throw std::runtime_error{"invalid material was referenced in the gmo model"};
        }

        const auto mesh_id =
            model->AddAnimatedMesh(vertices, ConvertIndicesGMO(gmo_draw_array), material_id.value(), skin_id.value());
        if (!mesh_id.has_value()) {
            throw std::runtime_error{"failed to allocate mesh"};
        }

        node.AddAnimMesh(mesh_id.value());
    };

    for (const auto gmo_bone_id : nodes_with_meshes) {
        const auto &gmo_bone = gmo_model.bones[gmo_bone_id];
        auto node_id = node_map[gmo_bone_id];
        auto &node = *model->GetNode(node_id.value());

        // if the node has blend bones and offsets
        // create a skin

        for (const auto gmo_part_id : gmo_bone.draw_parts) {
            const auto &gmo_part = gmo_model.parts[gmo_part_id];
            for (const auto &gmo_mesh : gmo_part.meshes) {
                for (const auto &gmo_draw_array : gmo_mesh.draw_arrays) {
                    const auto &gmo_vertex_array = gmo_part.vertex_arrays[gmo_draw_array.array_id];
                    const auto is_skinned =
                        gmo::FlagHas(gmo_vertex_array.flags, gmo::GmoVertexArrayFlags::eHasWeights) &&
                        !gmo_bone.blend_bones.empty();

                    if (is_skinned) {
                        // mesh is skinned, create a skin then
                        fnParseGmoSkinmesh(node, gmo_bone, gmo_mesh, gmo_draw_array, gmo_vertex_array);
                    } else {
                        fnParseGmoMesh(node, gmo_mesh, gmo_draw_array, gmo_vertex_array);
                    }
                }
            }
        }
    }

    // animation conversions
    for (uint32_t gmo_motion_id = 0; gmo_motion_id < gmo_model.motions.size(); ++gmo_motion_id) {
        const auto &gmo_motion = gmo_model.motions[gmo_motion_id];
        render::Model::Animation animation{gmo_motion.name};

        for (const auto &gmo_animation : gmo_motion.animations) {
            if (gmo_animation.target != gmo::GmoAnimationTarget::eBone) {
                throw std::runtime_error{"libconv: material animations are not supported"};
            }

            switch (gmo_animation.property) {
            case gmo::eAnimBoneTranslate: {
                if (gmo_animation.target_id >= node_map.size()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation {} has invalid target {}", gmo_animation.name,
                            gmo_animation.target_id));
                    continue;
                }

                const auto node_id = node_map[gmo_animation.target_id];
                if (!node_id.has_value()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation {} has invalid target {}", gmo_animation.name,
                            gmo_animation.target_id));
                    continue;
                }

                render::Model::Animation::TranslationChannel channel{node_id.value()};
                ConvertGMOKeyframes(channel, gmo_motion, gmo_animation);

                animation.AppendChannel(std::move(channel));
                break;
            }
            case gmo::eAnimBoneScale2: {
                if (gmo_animation.target_id >= node_map.size()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation {} has invalid target {}", gmo_animation.name,
                            gmo_animation.target_id));
                    continue;
                }

                const auto node_id = node_map[gmo_animation.target_id];
                if (!node_id.has_value()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation {} has invalid target {}", gmo_animation.name,
                            gmo_animation.target_id));
                    continue;
                }

                render::Model::Animation::ScaleChannel channel{node_id.value()};
                ConvertGMOKeyframes(channel, gmo_motion, gmo_animation);

                animation.AppendChannel(std::move(channel));
                break;
            }
            case gmo::eAnimBoneRotateQ: {
                if (gmo_animation.target_id >= node_map.size()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation {} has invalid target {}", gmo_animation.name,
                            gmo_animation.target_id));
                    continue;
                }

                const auto node_id = node_map[gmo_animation.target_id];
                if (!node_id.has_value()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation {} has invalid target {}", gmo_animation.name,
                            gmo_animation.target_id));
                    continue;
                }

                render::Model::Animation::RotationChannel channel{node_id.value()};
                ConvertGMOKeyframes(channel, gmo_motion, gmo_animation);

                animation.AppendChannel(std::move(channel));
                break;
            }
            case gmo::eAnimPataponTextureEXT: {
                // TODO
                break;
            }
            case gmo::eAnimPataponUnknownEXT: {
                // TODO
                break;
            }
            default:
                throw std::runtime_error{"libconv: unsupported target type"};
            }
        }

        model->AddAnimation(std::move(animation));
    }

    return model;
}

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

auto ConvertGXX(
    const gxx::GxxModel &gxx_model, render::hal::IDevice *device, const ITextureRepository *repository,
    const IConvertLogger *logger) -> std::unique_ptr<render::Model> {
    ConvertConsoleLogger console_logger;
    if (!logger) {
        logger = &console_logger;
    }

    using AnyMeshId = std::variant<render::Model::MeshId, render::Model::AnimatedMeshId, std::monostate>;

    auto model = std::make_unique<render::Model>(device);
    std::vector<render::Model::NodeId> bone_map;
    std::vector<render::Model::MaterialId> material_map;
    std::vector<AnyMeshId> mesh_map;

    for (const auto &gxx_bone : gxx_model.bones) {
        const auto bone_id = model->AddNode(model->GetRoot(), gxx_bone.name);
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
        };

        const auto material_id = model->AddMaterial(material_desc);
        if (!material_id.has_value()) {
            throw std::runtime_error{"failed to allocate model material"};
        }

        material_map.push_back(material_id.value());
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

            const auto mesh_id =
                gxx_mesh.primitive_type == gxx::eGxxPrimitiveTriangles
                    ? model->AddMesh(vertices, gxx_mesh.indices, material_map.front())
                    : model->AddMesh(
                          vertices, ConvertIndicesGXX(gxx_mesh.indices, gxx_mesh.primitive_type), material_map.front());
            if (!mesh_id.has_value()) {
                throw std::runtime_error{"failed to allocate model mesh"};
            }

            mesh_map.push_back(mesh_id.value());

            auto *target_bone = model->GetNode(bone_map[gxx_mesh_id]);
            if (target_bone) {
                target_bone->AddMesh(mesh_id.value());
            }
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
                }

                return vertex;
            });

            // since this is a skinned mesh, we add some bones parented to the root
            render::Model::Skin skin;
            for (uint32_t i = 0; i < gxx_mesh.num_weights; ++i) {
                const auto deform_bone_id =
                    model->AddNode(model->GetRoot(), fmt::format("m{:x}_deform{}", gxx_mesh.id, i));
                if (!deform_bone_id) {
                    throw std::runtime_error{"failed to allocate deform bone for mesh"};
                }

                skin.AddNodeRef(deform_bone_id.value(), glm::fmat4x4{1.0f});
            }

            const auto skin_id = model->AddSkin(std::move(skin));
            if (!skin_id.has_value()) {
                throw std::runtime_error{"failed to allocate deform skin for mesh"};
            }

            const auto mesh_id =
                gxx_mesh.primitive_type == gxx::eGxxPrimitiveTriangles
                    ? model->AddAnimatedMesh(vertices, gxx_mesh.indices, material_map.front(), skin_id.value())
                    : model->AddAnimatedMesh(
                          vertices, ConvertIndicesGXX(gxx_mesh.indices, gxx_mesh.primitive_type), material_map.front(),
                          skin_id.value());
            if (!mesh_id.has_value()) {
                throw std::runtime_error{"failed to allocate model animated mesh"};
            }

            mesh_map.push_back(mesh_id.value());

            auto *target_bone = model->GetNode(bone_map[gxx_mesh_id]);
            if (target_bone) {
                target_bone->AddAnimMesh(mesh_id.value());
            }
        }
    }

    return model;
}

inline auto ConvertInterpolation(act::AnimationInterpolationMode mode) -> render::Model::Animation::InterpolationMode {
    switch (mode) {
    case act::AnimationInterpolationMode::eStep:
        return render::Model::Animation::InterpolationMode::eStep;
    case act::AnimationInterpolationMode::eLinear:
        return render::Model::Animation::InterpolationMode::eLinear;
    case act::AnimationInterpolationMode::eCubicSpline:
        return render::Model::Animation::InterpolationMode::eCubic;
    default:
        return render::Model::Animation::InterpolationMode::eStep;
    }
}

auto ConvertACT(const act::Model &act_model, render::hal::IDevice *device, const IConvertLogger *logger)
    -> std::unique_ptr<render::Model> {
    ConvertConsoleLogger console_logger;
    if (!logger) {
        logger = &console_logger;
    }

    auto model = std::make_unique<render::Model>(device);

    const auto num_nodes = act_model.nodes.size();
    const auto num_textures = act_model.textures.size();
    const auto num_materials = act_model.materials.size();
    const auto num_submeshes = act_model.submeshes.size();

    using AnyMeshId = std::variant<render::Model::MeshId, render::Model::AnimatedMeshId, std::monostate>;

    std::vector<std::optional<render::Model::NodeId>> node_map;
    std::vector<std::optional<render::Model::TextureId>> texture_map;
    std::vector<std::optional<render::Model::MaterialId>> material_map;
    std::vector<AnyMeshId> submesh_map;

    node_map.resize(num_nodes);
    texture_map.resize(num_textures);
    material_map.resize(num_materials);
    submesh_map.resize(num_submeshes, std::monostate{});

    for (uint32_t act_texture_id = 0; act_texture_id < num_textures; ++act_texture_id) {
        const auto &act_texture = act_model.textures[act_texture_id];
        const auto &act_image = act_model.images[act_texture.image_id];

        std::vector<uint8_t> bitmap_buffer;

        const uint8_t *input_buffer_ptr = act_image.buffer.data();
        const auto input_buffer_size = static_cast<int>(act_image.buffer.size());
        int width = 0, height = 0, components = 0;

        int res = stbi_info_from_memory(input_buffer_ptr, input_buffer_size, &width, &height, &components);
        if (res) {
            auto img = stbi_load_from_memory(input_buffer_ptr, input_buffer_size, &width, &height, &components, 4);
            bitmap_buffer.resize(width * height * 4);

            ::memcpy(bitmap_buffer.data(), img, width * height * 4 * sizeof(uint8_t));
            stbi_image_free(img);

            auto texture_id =
                model->AddRgbaTexture(static_cast<uint32_t>(width), static_cast<uint32_t>(height), bitmap_buffer);
            texture_map[act_texture_id] = texture_id;
        } else {
            logger->Log(fmt::format("act image {} is invalid", act_texture.image_id));

            const auto backup_image = MakeErrorBitmap();
            auto texture_id = model->AddRgbaTexture(backup_image.width, backup_image.height, backup_image.plane);

            texture_map[act_texture_id] = texture_id;
        }
    }

    for (uint32_t act_material_id = 0; act_material_id < num_materials; ++act_material_id) {
        const auto &act_material = act_model.materials[act_material_id];
        render::Model::Material::Description desc;

        desc.diffuse =
            act_material.albedo_map_id.has_value() ? texture_map[act_material.albedo_map_id.value()] : std::nullopt;
        desc.normal =
            act_material.normal_map_id.has_value() ? texture_map[act_material.normal_map_id.value()] : std::nullopt;

        auto material_id = model->AddMaterial(desc);
        material_map[act_material_id] = material_id;
    }

    // node initialization
    std::queue<uint32_t> node_queue;
    for (uint32_t id = 0; id < num_nodes; ++id) {
        node_queue.push(id);
    }

    std::vector<int> nodes_with_meshes;

    for (uint32_t iter = 0; !node_queue.empty() && iter < num_nodes * num_nodes; ++iter) {
        auto act_node_id = node_queue.front();
        node_queue.pop();

        const auto &act_node = act_model.nodes[act_node_id];
        auto parent_id = model->GetRoot();

        if (act_node.parent_id.has_value()) {
            const auto act_parent_id = act_node.parent_id.value();
            if (!node_map[act_parent_id].has_value()) {
                node_queue.push(act_node_id);
                continue;
            }

            parent_id = node_map[act_parent_id].value();
        }

        const auto node_id = model->AddNode(parent_id, fmt::format("act_node_{}", act_node_id));
        if (!node_id.has_value()) {
            throw std::runtime_error{"failed to allocate node"};
        }

        node_map[act_node_id] = node_id;
        auto node = model->GetNode(node_id.value());

        node->SetTranslation(act_node.translation);
        node->SetRotation(act_node.rotation);
        node->SetScale(act_node.scale);

        if (act_node.mesh_id.has_value()) {
            nodes_with_meshes.push_back(act_node_id);
        }
    }

    if (!node_queue.empty()) {
        throw std::runtime_error{"act model has invalid topology"};
    }

    for (const auto act_node_id : nodes_with_meshes) {
        const auto &act_node = act_model.nodes[act_node_id];
        const auto &act_mesh = act_model.meshes[act_node.mesh_id.value()];

        auto node_id = node_map[act_node_id];
        auto &node = *model->GetNode(node_id.value());

        std::optional<render::Model::SkinId> skin_id = [&]() -> std::optional<render::Model::SkinId> {
            if (!act_node.skin_id.has_value()) {
                return std::nullopt;
            }

            const auto &act_skin = act_model.skins[act_node.skin_id.value()];

            render::Model::Skin skin;
            for (const auto &act_skin_node_id : act_skin.skin_node_ids) {
                const auto &act_skin_node = act_model.skin_nodes[act_skin_node_id];
                if (!node_map[act_skin_node.node_id].has_value()) {
                    return std::nullopt;
                }

                skin.AddNodeRef(node_map[act_skin_node.node_id].value(), act_skin_node.inverse_bind_matrix);
            }

            return model->AddSkin(std::move(skin));
        }();

        for (const auto act_submesh_id : act_mesh.submesh_ids) {
            const auto &act_any_submesh = act_model.submeshes[act_submesh_id];
            std::visit(
                overload{
                    [&](const act::Model::StaticSubmesh &act_submesh) {
                std::vector<render::StaticVertex> vertices{act_submesh.vertices.size()};
                for (size_t i = 0; i < act_submesh.vertices.size(); ++i) {
                    vertices[i].position = act_submesh.vertices[i].position;
                    vertices[i].normal = act_submesh.vertices[i].normal;
                    vertices[i].tangent = act_submesh.vertices[i].tangent;
                    vertices[i].uv = act_submesh.vertices[i].texcoord;
                    vertices[i].color = glm::fvec3{1.0f, 1.0f, 1.0f};
                }

                auto material_id = material_map[act_submesh.material];
                if (!material_id.has_value()) {
                    logger->Log(fmt::format("model: invalid material for act submesh {}", act_submesh_id));
                    return;
                }

                auto mesh_id = model->AddMesh(vertices, act_submesh.indices, material_id.value());
                submesh_map[act_submesh_id] = mesh_id.has_value() ? AnyMeshId{mesh_id.value()} : std::monostate{};

                if (mesh_id.has_value()) {
                    node.AddMesh(mesh_id.value());
                }
            },
                    [&](const act::Model::RiggedSubmesh &act_submesh) {
                // don't add animated submesh if skin is not present
                if (!skin_id.has_value()) {
                    return;
                }

                std::vector<render::AnimatedVertex> vertices{act_submesh.vertices.size()};
                for (size_t i = 0; i < act_submesh.vertices.size(); ++i) {
                    vertices[i].position = act_submesh.vertices[i].position;
                    vertices[i].normal = act_submesh.vertices[i].normal;
                    vertices[i].tangent = act_submesh.vertices[i].tangent;
                    vertices[i].uv = act_submesh.vertices[i].texcoord;
                    vertices[i].color = glm::fvec3{1.0f, 1.0f, 1.0f};

                    for (uint32_t w = 0; w < 4; ++w) {
                        vertices[i].bones[w] = act_submesh.vertices[i].joints[w];
                        vertices[i].weights[w] = act_submesh.vertices[i].weights[w];
                    }
                }

                auto material_id = material_map[act_submesh.material];
                if (!material_id.has_value()) {
                    logger->Log(fmt::format("model: invalid material for act submesh {}", act_submesh_id));
                    return;
                }

                auto mesh_id =
                    model->AddAnimatedMesh(vertices, act_submesh.indices, material_id.value(), skin_id.value());
                submesh_map[act_submesh_id] = mesh_id.has_value() ? AnyMeshId{mesh_id.value()} : std::monostate{};

                if (mesh_id.has_value()) {
                    node.AddAnimMesh(mesh_id.value());
                }
            },
                },
                act_any_submesh);
        }
    }

    // loading animations
    for (uint32_t act_anim_id = 0; act_anim_id < act_model.animations.size(); ++act_anim_id) {
        const auto &act_anim = act_model.animations[act_anim_id];

        render::Model::Animation animation{fmt::format("act_anim_{}", act_anim_id)};
        for (const auto act_channel_id : act_anim.channel_ids) {
            const auto &act_channel = act_model.animation_channels[act_channel_id];
            std::visit(
                overload{
                    [&](const act::Model::TranslationAnimationChannel &act_channel) {
                if (!node_map[act_channel.node_id].has_value()) {
                    return;
                }

                const auto node_id = node_map[act_channel.node_id].value();
                render::Model::Animation::TranslationChannel channel{node_id};

                channel.SetInterpolation(ConvertInterpolation(act_channel.interpolation));

                for (const auto &act_keyframe : act_channel.keyframes) {
                    channel.GetKeyframes().emplace_back(
                        render::Model::Animation::TranslationChannel::KeyframeType{
                            .value = act_keyframe.value,
                            .time = act_keyframe.time,
                        });
                }

                animation.AppendChannel(std::move(channel));
            },
                    [&](const act::Model::RotationAnimationChannel &act_channel) {
                if (!node_map[act_channel.node_id].has_value()) {
                    return;
                }

                const auto node_id = node_map[act_channel.node_id].value();
                render::Model::Animation::RotationChannel channel{node_id};

                channel.SetInterpolation(ConvertInterpolation(act_channel.interpolation));

                for (const auto &act_keyframe : act_channel.keyframes) {
                    channel.GetKeyframes().emplace_back(
                        render::Model::Animation::RotationChannel::KeyframeType{
                            .value = act_keyframe.value,
                            .time = act_keyframe.time,
                        });
                }

                animation.AppendChannel(std::move(channel));
            },
                    [&](const act::Model::ScaleAnimationChannel &act_channel) {
                if (!node_map[act_channel.node_id].has_value()) {
                    return;
                }

                const auto node_id = node_map[act_channel.node_id].value();
                render::Model::Animation::ScaleChannel channel{node_id};

                channel.SetInterpolation(ConvertInterpolation(act_channel.interpolation));

                for (const auto &act_keyframe : act_channel.keyframes) {
                    channel.GetKeyframes().emplace_back(
                        render::Model::Animation::ScaleChannel::KeyframeType{
                            .value = act_keyframe.value,
                            .time = act_keyframe.time,
                        });
                }

                animation.AppendChannel(std::move(channel));
            },
                },
                act_channel);
        }

        model->AddAnimation(std::move(animation));
    }

    return model;
}

} // namespace conv
