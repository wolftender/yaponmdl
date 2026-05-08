#include <algorithm>
#include <numeric>
#include <queue>

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
        return render::Model::Animation::InterpolationMode::eHermite;
    case gmo::GmoFCurveInterpolation::eCubic:
        return render::Model::Animation::InterpolationMode::eStep;
    }
}

template <render::Model::Animation::NodeTargetProperty Prop>
auto ConvertGMOKeyframes(
    render::Model::Animation::NodeChannel<Prop> &channel, const gmo::GmoMotion &gmo_motion,
    const gmo::GmoAnimation &gmo_animation) -> void {
    using DataType = render::Model::Animation::NodeChannel<Prop>::PropertyType;
    using KeyframeType = render::Model::Animation::NodeChannel<Prop>::KeyframeType;

    constexpr uint32_t kNumDimensions = DataType::length();
    // constexpr std::array<uint32_t, 5> kElementsPerInterpType = {1, 1, 3, 5, 1};

    const auto &gmo_fcurve = gmo_motion.fcurves[gmo_animation.fcurve_id];
    const auto num_dims_per_keyframe = gmo::NumElementsPerInterpType(gmo_fcurve.interpolation) * kNumDimensions + 1;

    if (num_dims_per_keyframe * gmo_fcurve.num_keyframes > gmo_fcurve.raw_data.size()) {
        throw std::runtime_error{"invalid gmo fcurve, dimensions don't match"};
    }

    channel.SetInterpolation(ConvertGMOInterpolation(gmo_fcurve.interpolation));
    const auto fps = static_cast<float>(gmo_motion.framerate);

    switch (gmo_fcurve.interpolation) {
    case gmo::GmoFCurveInterpolation::eConstant:
    case gmo::GmoFCurveInterpolation::eLinear:
    case gmo::GmoFCurveInterpolation::eSpherical:
        for (uint32_t keyframe_idx = 0; keyframe_idx < gmo_fcurve.num_keyframes; ++keyframe_idx) {
            const auto base_idx = num_dims_per_keyframe * keyframe_idx;

            KeyframeType keyframe;
            keyframe.time = gmo_fcurve.raw_data[base_idx] / fps;

            for (uint32_t d = 0; d < kNumDimensions; ++d) {
                keyframe.value[d] = gmo_fcurve.raw_data[base_idx + 1 + d];
            }

            channel.GetKeyframes().emplace_back(std::move(keyframe));
        }

        break;

    case gmo::GmoFCurveInterpolation::eHermite:
        for (uint32_t keyframe_idx = 0; keyframe_idx < gmo_fcurve.num_keyframes; ++keyframe_idx) {
            const auto base_idx = num_dims_per_keyframe * keyframe_idx;

            KeyframeType keyframe;
            keyframe.time = gmo_fcurve.raw_data[base_idx] / fps;

            for (uint32_t d = 0; d < kNumDimensions; ++d) {
                keyframe.value[d] = gmo_fcurve.raw_data[base_idx + (3 * d) + 1];
                keyframe.in_dy[d] = gmo_fcurve.raw_data[base_idx + (3 * d) + 2];
                keyframe.out_dy[d] = gmo_fcurve.raw_data[base_idx + (3 * d) + 3];
            }

            channel.GetKeyframes().emplace_back(std::move(keyframe));
        }

        break;

    case gmo::GmoFCurveInterpolation::eCubic:
        for (uint32_t keyframe_idx = 0; keyframe_idx < gmo_fcurve.num_keyframes; ++keyframe_idx) {
            const auto base_idx = num_dims_per_keyframe * keyframe_idx;

            KeyframeType keyframe;
            keyframe.time = gmo_fcurve.raw_data[base_idx] / fps;

            for (uint32_t d = 0; d < kNumDimensions; ++d) {
                keyframe.value[d] = gmo_fcurve.raw_data[base_idx + (5 * d) + 1];
            }

            channel.GetKeyframes().emplace_back(std::move(keyframe));
        }

        break;
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
        desc.color = gmo_material.colors[gmo::eGmoMaterialColorDiffuse];

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
                logger->Log(fmt::format("libconv: motion track {} has an unsupported target type", gmo_motion.name));
                continue;
            }

            using NodeTargetProperty = render::Model::Animation::NodeTargetProperty;

            switch (gmo_animation.property) {
            case gmo::eAnimBoneScale1:
            case gmo::eAnimBoneScale3:
                logger->Log(
                    fmt::format(
                        "libconv: warning! scale 1 or scale 3 is used for this models animation track {}",
                        gmo_animation.name));
                break;

            case gmo::eAnimBoneTranslate: {
                if (gmo_animation.target_id >= node_map.size()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation \"{}\" in motion {} has invalid target {} ", gmo_animation.name,
                            gmo_motion.name, gmo_animation.target_id));
                    continue;
                }

                const auto node_id = node_map[gmo_animation.target_id];
                if (!node_id.has_value()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation \"{}\" in motion {} has invalid target {} ", gmo_animation.name,
                            gmo_motion.name, gmo_animation.target_id));
                    continue;
                }

                animation.AppendNodeChannel<NodeTargetProperty::eTranslation>(
                    node_id.value(), [&](auto &translation_channel) {
                    ConvertGMOKeyframes<NodeTargetProperty::eTranslation>(
                        translation_channel, gmo_motion, gmo_animation);
                });

                break;
            }
            case gmo::eAnimBoneScale2: {
                if (gmo_animation.target_id >= node_map.size()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation \"{}\" in motion {} has invalid target {} ", gmo_animation.name,
                            gmo_motion.name, gmo_animation.target_id));
                    continue;
                }

                const auto node_id = node_map[gmo_animation.target_id];
                if (!node_id.has_value()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation \"{}\" in motion {} has invalid target {} ", gmo_animation.name,
                            gmo_motion.name, gmo_animation.target_id));
                    continue;
                }

                animation.AppendNodeChannel<NodeTargetProperty::eScale>(node_id.value(), [&](auto &scale_channel) {
                    ConvertGMOKeyframes<NodeTargetProperty::eScale>(scale_channel, gmo_motion, gmo_animation);
                });

                break;
            }
            case gmo::eAnimBoneRotateQ: {
                if (gmo_animation.target_id >= node_map.size()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation \"{}\" in motion {} has invalid target {} ", gmo_animation.name,
                            gmo_motion.name, gmo_animation.target_id));
                    continue;
                }

                const auto node_id = node_map[gmo_animation.target_id];
                if (!node_id.has_value()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation \"{}\" in motion {} has invalid target {} ", gmo_animation.name,
                            gmo_motion.name, gmo_animation.target_id));
                    continue;
                }

                animation.AppendNodeChannel<NodeTargetProperty::eRotation>(
                    node_id.value(), [&](auto &rotation_channel) {
                    ConvertGMOKeyframes<NodeTargetProperty::eRotation>(rotation_channel, gmo_motion, gmo_animation);
                });

                break;
            }
            case gmo::eAnimPataponTextureEXT: {
                if (gmo_animation.target_id >= node_map.size()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation \"{}\" in motion {} has invalid target {} ", gmo_animation.name,
                            gmo_motion.name, gmo_animation.target_id));
                    continue;
                }

                const auto node_id = node_map[gmo_animation.target_id];
                if (!node_id.has_value()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation \"{}\" in motion {} has invalid target {} ", gmo_animation.name,
                            gmo_motion.name, gmo_animation.target_id));
                    continue;
                }

                animation.AppendNodeChannel<NodeTargetProperty::eUvOffset>(
                    node_id.value(), [&](auto &uv_offset_channel) {
                    ConvertGMOKeyframes<NodeTargetProperty::eUvOffset>(uv_offset_channel, gmo_motion, gmo_animation);
                });

                break;
            }
            case gmo::eAnimPataponUnknownEXT: {
                if (gmo_animation.target_id >= node_map.size()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation \"{}\" in motion {} has invalid target {} ", gmo_animation.name,
                            gmo_motion.name, gmo_animation.target_id));
                    continue;
                }

                const auto node_id = node_map[gmo_animation.target_id];
                if (!node_id.has_value()) {
                    logger->Log(
                        fmt::format(
                            "libconv: animation \"{}\" in motion {} has invalid target {} ", gmo_animation.name,
                            gmo_motion.name, gmo_animation.target_id));
                    continue;
                }

                animation.AppendNodeChannel<NodeTargetProperty::eAlpha>(node_id.value(), [&](auto &alpha_channel) {
                    ConvertGMOKeyframes<NodeTargetProperty::eAlpha>(alpha_channel, gmo_motion, gmo_animation);
                });

                break;
            }
            default:
                throw std::runtime_error{"libconv: unsupported target type"};
            }
        }

        animation.SetDuration(gmo_motion.frame_loop_end / gmo_motion.framerate);
        model->AddAnimation(std::move(animation));
    }

    return model;
}

} // namespace conv
