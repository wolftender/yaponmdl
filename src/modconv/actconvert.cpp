#include <queue>
#include <variant>

#include <fmt/format.h>
#include <stb_image/stb_image.h>

#include "common/common.hpp"
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

} // namespace

namespace conv {

inline auto ConvertInterpolation(act::ActInterpolationMode mode) -> render::Model::Animation::InterpolationMode {
    switch (mode) {
    case act::ActInterpolationMode::eStep:
        return render::Model::Animation::InterpolationMode::eStep;
    case act::ActInterpolationMode::eLinear:
        return render::Model::Animation::InterpolationMode::eLinear;
    case act::ActInterpolationMode::eCubicSpline:
        return render::Model::Animation::InterpolationMode::eCubic;
    case act::ActInterpolationMode::eSlerp:
        return render::Model::Animation::InterpolationMode::eLinear;
    case act::ActInterpolationMode::eHermite:
        return render::Model::Animation::InterpolationMode::eHermite;
    default:
        return render::Model::Animation::InterpolationMode::eStep;
    }
}

template <act::ActNodeProperty ActProperty> struct ActPropertyToProperty;
template <> struct ActPropertyToProperty<act::ActNodeProperty::eTranslation> {
    static constexpr auto Property = render::Model::Animation::NodeTargetProperty::eTranslation;
};

template <> struct ActPropertyToProperty<act::ActNodeProperty::eRotation> {
    static constexpr auto Property = render::Model::Animation::NodeTargetProperty::eRotation;
};

template <> struct ActPropertyToProperty<act::ActNodeProperty::eScale> {
    static constexpr auto Property = render::Model::Animation::NodeTargetProperty::eScale;
};

template <> struct ActPropertyToProperty<act::ActNodeProperty::eUvOffset> {
    static constexpr auto Property = render::Model::Animation::NodeTargetProperty::eUvOffset;
};

template <> struct ActPropertyToProperty<act::ActNodeProperty::eUvScale> {
    static constexpr auto Property = render::Model::Animation::NodeTargetProperty::eUvScale;
};

template <> struct ActPropertyToProperty<act::ActNodeProperty::eAlpha> {
    static constexpr auto Property = render::Model::Animation::NodeTargetProperty::eAlpha;
};

template <render::Model::Animation::NodeTargetProperty Property, act::ActNodeProperty ActProperty>
    requires std::is_same_v<
        typename render::Model::Animation::NodePropertyDataType<Property>::Type,
        typename act::ActAnimPropertyTraits<ActProperty>::DataType>
auto ConvertChannel(
    render::Model::Animation::NodeChannel<Property> &channel, const act::ActAnimationChannel<ActProperty> &act_channel)
    -> void {

    using KeyframeType = render::Model::Animation::NodeChannel<Property>::KeyframeType;

    channel.SetInterpolation(ConvertInterpolation(act_channel.interpolation));

    auto &keyframes = channel.GetKeyframes();
    const auto &act_keyframes = act_channel.keyframes;

    keyframes.reserve(act_keyframes.size());

    switch (act_channel.interpolation) {
    case act::ActInterpolationMode::eHermite:
        std::transform(
            act_keyframes.begin(), act_keyframes.end(), std::back_inserter(keyframes), [&](const auto act_keyframe) {
            return KeyframeType{
                .time = act_keyframe.time,
                .value = act_keyframe.value,
                .in_dy = act_keyframe.in_dy,
                .out_dy = act_keyframe.out_dy,
            };
        });
        break;
    default:
        std::transform(
            act_keyframes.begin(), act_keyframes.end(), std::back_inserter(keyframes), [&](const auto act_keyframe) {
            return KeyframeType{
                .time = act_keyframe.time,
                .value = act_keyframe.value,
            };
        });
        break;
    }
}

auto ConvertACT(const act::ActModel &act_model, render::hal::IDevice *device, const IConvertLogger *logger)
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
    const auto num_skins = act_model.skins.size();

    using AnyMeshId = std::variant<render::Model::MeshId, render::Model::AnimatedMeshId, std::monostate>;

    std::vector<std::optional<render::Model::NodeId>> node_map;
    std::vector<std::optional<render::Model::TextureId>> texture_map;
    std::vector<std::optional<render::Model::MaterialId>> material_map;
    std::vector<std::optional<render::Model::SkinId>> skin_map;
    std::vector<AnyMeshId> submesh_map;

    node_map.resize(num_nodes);
    texture_map.resize(num_textures);
    material_map.resize(num_materials);
    skin_map.resize(num_skins);
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
        desc.color = act_material.base_color;
        desc.uv_offset = act_material.uv_offset;
        desc.uv_scale = act_material.uv_scale;

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

        render::Model::Node::Description node_description = {
            .translation = act_node.translation,
            .scale = act_node.scale,
            .rotation = act_node.rotation,
            .draw_sort = act_node.draw_order,
            .bounds =
                {
                    .min = glm::fvec3{-3.0f, -10.0f, -3.0f},
                    .max = glm::fvec3{3.0f, 10.0f, 3.0f},
                },
        };

        const auto node_id = model->AddNode(parent_id, act_node.name, node_description);
        if (!node_id.has_value()) {
            throw std::runtime_error{"failed to allocate node"};
        }

        node_map[act_node_id] = node_id;

        if (act_node.mesh_id.has_value()) {
            nodes_with_meshes.push_back(act_node_id);
        }
    }

    if (!node_queue.empty()) {
        throw std::runtime_error{"act model has invalid topology"};
    }

    for (uint32_t act_skin_id = 0; act_skin_id < num_skins; ++act_skin_id) {
        const auto &act_skin = act_model.skins[act_skin_id];

        render::Model::Skin skin;
        for (const auto &act_skin_node_id : act_skin.skin_node_ids) {
            const auto &act_skin_node = act_model.skin_nodes[act_skin_node_id];
            if (!node_map[act_skin_node.node_id].has_value()) {
                throw std::runtime_error{fmt::format("skin references invalid node id {}", act_skin_node.node_id)};
            }

            skin.AddNodeRef(node_map[act_skin_node.node_id].value(), act_skin_node.inverse_bind_matrix);
        }

        auto skin_id = model->AddSkin(std::move(skin));
        skin_map[act_skin_id] = skin_id;
    }

    for (uint32_t act_skin_id = 0; act_skin_id < num_skins; ++act_skin_id) {
        const auto &act_skin = act_model.skins[act_skin_id];

        render::Model::Skin skin;
        for (const auto &act_skin_node_id : act_skin.skin_node_ids) {
            const auto &act_skin_node = act_model.skin_nodes[act_skin_node_id];
            if (!node_map[act_skin_node.node_id].has_value()) {
                throw std::runtime_error{fmt::format("skin references invalid node id {}", act_skin_node.node_id)};
            }

            skin.AddNodeRef(node_map[act_skin_node.node_id].value(), act_skin_node.inverse_bind_matrix);
        }

        auto skin_id = model->AddSkin(std::move(skin));
        skin_map[act_skin_id] = skin_id;
    }

    for (const auto act_node_id : nodes_with_meshes) {
        const auto &act_node = act_model.nodes[act_node_id];
        const auto &act_mesh = act_model.meshes[act_node.mesh_id.value()];

        auto node_id = node_map[act_node_id];
        auto &node = *model->GetNode(node_id.value());

        for (const auto act_submesh_id : act_mesh.submesh_ids) {
            const auto &act_any_submesh = act_model.submeshes[act_submesh_id];
            std::visit(
                overload{
                    [&](const act::ActStaticSubmesh &act_submesh) {
                std::vector<render::StaticVertex> vertices{act_submesh.vertices.size()};
                for (size_t i = 0; i < act_submesh.vertices.size(); ++i) {
                    vertices[i].position = act_submesh.vertices[i].position;
                    vertices[i].uv = act_submesh.vertices[i].texcoord;
                    vertices[i].color = act_submesh.vertices[i].color;
                }

                auto material_id = material_map[act_submesh.material];
                if (!material_id.has_value()) {
                    logger->Log(fmt::format("error: invalid material for act submesh {}", act_submesh_id));
                    return;
                }

                auto mesh_id = model->AddMesh(vertices, act_submesh.indices, material_id.value());
                submesh_map[act_submesh_id] = mesh_id.has_value() ? AnyMeshId{mesh_id.value()} : std::monostate{};

                if (mesh_id.has_value()) {
                    node.AddMesh(mesh_id.value());
                }
            },
                    [&](const act::ActSkinnedSubmesh &act_submesh) {
                // don't add animated submesh if skin is not present
                const auto skin_id =
                    act_submesh.skin_id.has_value()
                        ? skin_map[act_submesh.skin_id.value()]
                        : (act_node.skin_id.has_value() ? skin_map[act_node.skin_id.value()] : std::nullopt);

                if (!skin_id.has_value()) {
                    return;
                }

                std::vector<render::AnimatedVertex> vertices{act_submesh.vertices.size()};
                for (size_t i = 0; i < act_submesh.vertices.size(); ++i) {
                    vertices[i].position = act_submesh.vertices[i].position;
                    vertices[i].uv = act_submesh.vertices[i].texcoord;
                    vertices[i].color = act_submesh.vertices[i].color;

                    for (uint32_t w = 0; w < 8; ++w) {
                        vertices[i].bones[w] = act_submesh.vertices[i].joints[w];
                        vertices[i].weights[w] = act_submesh.vertices[i].weights[w];
                    }
                }

                auto material_id = material_map[act_submesh.material];
                if (!material_id.has_value()) {
                    logger->Log(fmt::format("error: invalid material for act submesh {}", act_submesh_id));
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

        render::Model::Animation animation{
            act_anim.name.empty() ? fmt::format("act_anim_{}", act_anim_id) : act_anim.name};

        for (const auto act_channel_id : act_anim.channel_ids) {
            const auto &act_channel = act_model.animation_channels[act_channel_id];
            std::visit([&](const auto &typed_act_channel) -> void {
                const auto node_id = node_map[typed_act_channel.node_id].value();
                constexpr auto Property = ActPropertyToProperty<
                    std::remove_reference<decltype(typed_act_channel)>::type::ActProperty>::Property;
                animation.AppendNodeChannel<Property>(node_id, [&](auto &mut_model_channel) -> void {
                    ConvertChannel(mut_model_channel, typed_act_channel);
                });
            }, act_channel);
        }

        animation.SetDuration(act_anim.frame_loop.y);
        model->AddAnimation(std::move(animation));
    }

    return model;
}

} // namespace conv