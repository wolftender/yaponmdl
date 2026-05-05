#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/spline.hpp>

#include "render/model.hpp"

namespace render {

auto Model::Node::AddMesh(MeshId id) -> void {
    const auto is_first_drawable = (meshes_.size() == 0) && (animated_meshes_.size() == 0);
    meshes_.push_back(std::move(id));

    if (is_first_drawable) {
        model_->mesh_nodes_.push_back(self_);
    }
}

auto Model::Node::AddAnimMesh(AnimatedMeshId id) -> void {
    const auto is_first_drawable = (meshes_.size() == 0) && (animated_meshes_.size() == 0);
    animated_meshes_.push_back(std::move(id));

    if (is_first_drawable) {
        model_->mesh_nodes_.push_back(self_);
    }
}

auto Model::Pose::Node::SetTranslation(const glm::fvec3 &translation) -> void {
    translation_ = translation;
    pose_->RecomputeTransformSubtree(self_);
}

auto Model::Pose::Node::SetScale(const glm::fvec3 &scale) -> void {
    scale_ = scale;
    pose_->RecomputeTransformSubtree(self_);
}

auto Model::Pose::Node::SetRotation(const glm::fquat &rotation) -> void {
    rotation_ = rotation;
    pose_->RecomputeTransformSubtree(self_);
}

auto Model::Pose::Node::SetTranslationSilent(const glm::fvec3 &translation) -> void { translation_ = translation; }
auto Model::Pose::Node::SetScaleSilent(const glm::fvec3 &scale) -> void { scale_ = scale; }
auto Model::Pose::Node::SetRotationSilent(const glm::fquat &rotation) -> void { rotation_ = rotation; }

auto Model::Pose::Node::SetTransform(const glm::fvec3 &translation, const glm::fvec3 &scale, const glm::fquat &rotation)
    -> void {
    translation_ = translation;
    scale_ = scale;
    rotation_ = rotation;

    pose_->RecomputeTransformSubtree(self_);
}

auto Model::Pose::GetNode(NodeId handle) -> Node * {
    if (handle.index() >= nodes_.size()) {
        return nullptr;
    }

    return &nodes_[handle.index()];
}

auto Model::Pose::GetNode(NodeId handle) const -> const Node * {
    if (handle.index() >= nodes_.size()) {
        return nullptr;
    }

    return &nodes_[handle.index()];
}

auto Model::Pose::Reset(const Model &model) -> void {
    for (auto &pose_node : nodes_) {
        const auto *node = model.GetNode(pose_node.GetSelf());

        pose_node.translation_ = node->GetTranslation();
        pose_node.scale_ = node->GetScale();
        pose_node.rotation_ = node->GetRotation();
        pose_node.parent_ = node->GetParent();
        pose_node.children_ = node->GetChildren();
        pose_node.color_ = node->GetColor();
        pose_node.uv_offset_ = node->GetUvOffset();
        pose_node.uv_scale_ = node->GetUvScale();
        pose_node.alpha_ = node->GetAlpha();
    }
}

auto Model::Pose::FromModel(const Model &model) -> std::unique_ptr<Pose> {
    auto pose = std::unique_ptr<Pose>(new Pose());

    pose->device_ = model.device_;
    pose->nodes_.reserve(model.nodes_.size());

    for (const auto &node : model.nodes_) {
        Pose::Node pose_node{pose.get(), node.GetSelf()};

        pose_node.translation_ = node.GetTranslation();
        pose_node.scale_ = node.GetScale();
        pose_node.rotation_ = node.GetRotation();
        pose_node.parent_ = node.GetParent();
        pose_node.children_ = node.GetChildren();
        pose_node.color_ = node.GetColor();
        pose_node.uv_offset_ = node.GetUvOffset();
        pose_node.uv_scale_ = node.GetUvScale();
        pose_node.alpha_ = node.GetAlpha();

        pose->nodes_.emplace_back(std::move(pose_node));
    }

    pose->RecomputeTransformSubtree(pose->GetRoot());

    for (const auto &anim_mesh : model.animated_meshes_) {
        auto skinning_buffer_handle = pose->GetDevice().CreateSkinningBuffer();

        // fill with bind pose matrices
        const auto &skin = model.skins_[anim_mesh.GetSkin().index()];

        for (uint32_t i = 0; i < skin.GetNodes().size(); ++i) {
            pose->device_->UpdateSkinningBuffer(skinning_buffer_handle, i, glm::fmat4x4{1.0f});
        }

        pose->skinning_buffers_.emplace_back(std::move(skinning_buffer_handle));
    }

    return pose;
}

auto Model::Pose::RecomputeTransformSubtree(NodeId root) -> void {
    glm::fmat4x4 parent_matrix{1.0f};

    auto node = GetNode(root);
    if (!node) {
        return;
    }

    if (node->parent_.has_value()) {
        const auto parent = GetNode(node->parent_.value());
        if (parent) {
            parent_matrix = parent->GetTransform();
        }
    }

    // T * R * S
    const auto translation = glm::translate(glm::fmat4x4{1.0f}, node->GetTranslation());
    const auto rotation = glm::toMat4(node->GetRotation());
    const auto scale = glm::scale(glm::fmat4x4{1.0f}, node->GetScale());

    node->transform_ = parent_matrix * translation * rotation * scale;

    for (auto &child : node->children_) {
        RecomputeTransformSubtree(child);
    }
}

Model::Pose::~Pose() noexcept {}

Model::Model(hal::IDevice *device) : device_{device} {
    nodes_.emplace_back(
        Node{
            this,
            NodeId{0ul},
            "(root)",
            std::nullopt,
            /* translation = */ glm::fvec3{0.0f, 0.0f, 0.0f},
            /* scale       = */ glm::fvec3{1.0f, 1.0f, 1.0f},
            /* rotation    = */ glm::fquat{1.0f, 0.0f, 0.0f, 0.0f},
            /* color       = */ glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f},
            /* uv_offset   = */ glm::fvec2{0.0f, 0.0f},
            /* uv_scale    = */ glm::fvec2{1.0f, 1.0f},
            /* alpha       = */ glm::fvec1{1.0f},
        });
}

auto Model::CreatePose() const -> std::unique_ptr<Pose> { return Pose::FromModel(*this); }

auto Model::AddMeshImpl(std::span<const StaticVertex> vertices, std::span<const uint32_t> indices, MaterialId material)
    -> std::optional<MeshId> {
    auto mesh = device_->CreateMesh(vertices, indices);

    meshes_.emplace_back(Mesh{this, std::move(mesh), material});
    return MeshId{static_cast<uint32_t>(meshes_.size() - 1)};
}

auto Model::AddAnimMeshImpl(
    std::span<const AnimatedVertex> vertices, std::span<const uint32_t> indices, MaterialId material, SkinId skin)
    -> std::optional<AnimatedMeshId> {
    auto mesh = device_->CreateAnimatedMesh(vertices, indices);

    animated_meshes_.emplace_back(AnimatedMesh{this, std::move(mesh), material, std::move(skin)});
    return AnimatedMeshId{static_cast<uint32_t>(animated_meshes_.size()) - 1};
}

auto Model::AddRgbaTextureImpl(uint32_t width, uint32_t height, std::span<const uint8_t> data)
    -> std::optional<TextureId> {
    hal::TextureDescription desc = {
        .width = width,
        .height = height,
        .min_filter = hal::TextureMinFilter::eLinear,
        .mag_filter = hal::TextureMagFilter::eLinear,
        .wrap_s = hal::TextureWrap::eRepeat,
        .wrap_t = hal::TextureWrap::eRepeat,
        .data = data,
    };

    auto texture = device_->CreateRgbaTexture(desc);

    textures_.emplace_back(Texture{this, std::move(texture)});
    return TextureId{static_cast<uint32_t>(textures_.size() - 1)};
}

auto Model::AddSkin(Skin &&skin) -> std::optional<SkinId> {
    skins_.emplace_back(std::move(skin));
    return SkinId{static_cast<uint32_t>(skins_.size() - 1)};
}

auto Model::AddMaterial(const Material::Description &desc) -> std::optional<MaterialId> {
    materials_.emplace_back(Material{this, desc});
    return MaterialId{static_cast<uint32_t>(materials_.size() - 1)};
}

auto Model::AddNode(NodeId parent, std::string_view name) -> std::optional<NodeId> {
    auto node = GetNode(parent);
    if (!node) {
        return std::nullopt;
    }

    NodeId id{static_cast<uint32_t>(nodes_.size())};
    nodes_.emplace_back(
        Node{
            this,
            id,
            name,
            parent,
            /* translation = */ glm::fvec3{0.0f, 0.0f, 0.0f},
            /* scale       = */ glm::fvec3{1.0f, 1.0f, 1.0f},
            /* rotation    = */ glm::fquat{1.0f, 0.0f, 0.0f, 0.0f},
            /* color       = */ glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f},
            /* uv_offset   = */ glm::fvec2{0.0f, 0.0f},
            /* uv_scale    = */ glm::fvec2{1.0f, 1.0f},
            /* alpha       = */ glm::fvec1{1.0f},
        });

    node = GetNode(parent);
    node->children_.push_back(id);

    return id;
}

auto Model::AddAnimation(Animation &&animation) -> std::optional<AnimationId> {
    animations_.emplace_back(std::move(animation));
    return AnimationId{static_cast<uint32_t>(animations_.size())};
}

auto Model::GetMesh(MeshId handle) -> Mesh * {
    if (handle.index() >= meshes_.size()) {
        return nullptr;
    }

    return &meshes_[handle.index()];
}

auto Model::GetAnimMesh(AnimatedMeshId handle) -> AnimatedMesh * {
    if (handle.index() >= animated_meshes_.size()) {
        return nullptr;
    }

    return &animated_meshes_[handle.index()];
}

auto Model::GetNode(NodeId handle) -> Node * {
    if (handle.index() >= nodes_.size()) {
        return nullptr;
    }

    return &nodes_[handle.index()];
}

auto Model::GetTexture(TextureId handle) -> Texture * {
    if (handle.index() >= textures_.size()) {
        return nullptr;
    }

    return &textures_[handle.index()];
}

auto Model::GetMaterial(MaterialId handle) -> Material * {
    if (handle.index() >= materials_.size()) {
        return nullptr;
    }

    return &materials_[handle.index()];
}

auto Model::GetAnimation(AnimationId handle) -> Animation * {
    if (handle.index() >= animations_.size()) {
        return nullptr;
    }

    return &animations_[handle.index()];
}

auto Model::GetSkin(SkinId handle) -> Skin * {
    if (handle.index() > skins_.size()) {
        return nullptr;
    }

    return &skins_[handle.index()];
}

auto Model::GetMesh(MeshId handle) const -> const Mesh * {
    if (handle.index() >= meshes_.size()) {
        return nullptr;
    }

    return &meshes_[handle.index()];
}

auto Model::GetAnimMesh(AnimatedMeshId handle) const -> const AnimatedMesh * {
    if (handle.index() >= animated_meshes_.size()) {
        return nullptr;
    }

    return &animated_meshes_[handle.index()];
}

auto Model::GetNode(NodeId handle) const -> const Node * {
    if (handle.index() >= nodes_.size()) {
        return nullptr;
    }

    return &nodes_[handle.index()];
}

auto Model::GetTexture(TextureId handle) const -> const Texture * {
    if (handle.index() >= textures_.size()) {
        return nullptr;
    }

    return &textures_[handle.index()];
}

auto Model::GetMaterial(MaterialId handle) const -> const Material * {
    if (handle.index() >= materials_.size()) {
        return nullptr;
    }

    return &materials_[handle.index()];
}

auto Model::GetAnimation(AnimationId handle) const -> const Animation * {
    if (handle.index() >= animations_.size()) {
        return nullptr;
    }

    return &animations_[handle.index()];
}

auto Model::GetSkin(SkinId handle) const -> const Skin * {
    if (handle.index() > skins_.size()) {
        return nullptr;
    }

    return &skins_[handle.index()];
}

auto Model::Render(Pose &pose, const glm::fmat4x4 &world) const -> void {
    for (const auto &node_id : mesh_nodes_) {
        const auto &node = nodes_[node_id.index()];
        const auto &pose_node = pose.nodes_[node_id.index()];
        const auto matrix = world * pose_node.GetTransform();

        for (const auto &mesh_id : node.GetMeshes()) {
            const auto &mesh = meshes_[mesh_id.index()];
            const auto &material = materials_[mesh.GetMaterial().index()];

            hal::StaticDrawDescription draw_desc = {
                .mesh = mesh.GetHandle(),
                .world_matrix = matrix,
                .color = material.GetColor() * pose_node.GetColor(),
                .uv_offset = pose_node.GetUvOffset(),
                .uv_scale = pose_node.GetUvScale(),
                .alpha = pose_node.GetAlpha(),
                .diffuse_map = std::nullopt,
                .normal_map = std::nullopt,
            };

            if (material.GetDiffuse().has_value()) {
                draw_desc.diffuse_map = textures_[material.GetDiffuse()->index()].GetHandle();
            }

            if (material.GetNormal().has_value()) {
                draw_desc.normal_map = textures_[material.GetNormal()->index()].GetHandle();
            }

            device_->SubmitStaticDraw(std::move(draw_desc));
        }

        for (const auto &anim_mesh_id : node.GetAnimMeshes()) {
            const auto &anim_mesh = animated_meshes_[anim_mesh_id.index()];
            const auto &material = materials_[anim_mesh.GetMaterial().index()];

            // this is an animated mesh -> we need to update the matrices in the buffer
            const auto &skin = skins_[anim_mesh.skin_.index()];

            for (uint32_t bone_id = 0; bone_id < skin.GetNodes().size(); ++bone_id) {
                const auto &bone = skin.GetNodes()[bone_id];
                const auto &bone_node = pose.nodes_[bone.node.index()];
                glm::fmat4x4 joint_matrix = bone_node.GetTransform() * bone.inverse_bind;

                device_->UpdateSkinningBuffer(pose.skinning_buffers_[anim_mesh_id.index()], bone_id, joint_matrix);
            }

            hal::SkinnedDrawDescription draw_desc = {
                .mesh = anim_mesh.GetHandle(),
                .skinning_buffer = pose.skinning_buffers_[anim_mesh_id.index()],
                .world_matrix = world,
                .color = material.GetColor() * pose_node.GetColor(),
                .uv_offset = pose_node.GetUvOffset(),
                .uv_scale = pose_node.GetUvScale(),
                .alpha = pose_node.GetAlpha(),
                .diffuse_map = std::nullopt,
                .normal_map = std::nullopt,
            };

            if (material.GetDiffuse().has_value()) {
                draw_desc.diffuse_map = textures_[material.GetDiffuse()->index()].GetHandle();
            }

            if (material.GetNormal().has_value()) {
                draw_desc.normal_map = textures_[material.GetNormal()->index()].GetHandle();
            }

            device_->SubmitSkinnedDraw(std::move(draw_desc));
        }
    }
}

auto Model::Animation::AnyNodeChannel::Start([[maybe_unused]] Pose &pose, Data &animation_data) const -> void {
    const auto num_keyframes = GetNumKeyframes();

    if (num_keyframes == 0) { // invalid channel dont initialize?
        return;
    }

    if (num_keyframes == 1) { // one keyframe, simply add it
        animation_data.prev_keyframe_ = 0;
        animation_data.next_keyframe_ = 0;
        animation_data.prev_keyframe_time_ = 0.0f;
        animation_data.next_keyframe_time_ = 99999.0f;
        return;
    }

    // otherwise pick the first keyframe and the second keyframe
    animation_data.prev_keyframe_ = 0;
    animation_data.next_keyframe_ = 1;
    animation_data.prev_keyframe_time_ = GetKeyframeTime(0);
    animation_data.next_keyframe_time_ = GetKeyframeTime(1);
}

auto Model::Animation::AnyNodeChannel::Apply(float time, Pose &pose, Data &animation_data) const -> void {
    auto node = pose.GetNode(GetNode());
    if (!node) {
        return;
    }

    Apply(
        time, node, &animation_data.prev_keyframe_time_, &animation_data.next_keyframe_time_,
        &animation_data.prev_keyframe_, &animation_data.next_keyframe_);
}

auto Model::Controller::SetAnimation(AnimationId id) -> void {
    animation_ = id;
    time_ = 0.0f;
    node_channel_data_.clear();

    InitializeAnimationData();
}

auto Model::Controller::Integrate(float delta_time) -> void {
    time_ = time_ + delta_time;

    auto animation = model_->GetAnimation(animation_);
    if (animation) {
        if (loop_ && time_ > animation->GetDuration()) {
            time_ = std::fmod(time_, animation->GetDuration());
            ResetAnimationData(*animation);
        }

        UpdateAnimation(*animation);
    }
}

auto Model::Controller::Seek(float time) -> void {
    auto animation = model_->GetAnimation(animation_);
    if (animation) {
        time_ = loop_ ? std::fmod(std::max(0.0f, time), animation->GetDuration()) : std::max(0.0f, time);
        ResetAnimationData(*animation);
        UpdateAnimation(*animation);
    }
}

auto Model::Controller::ResetAnimationData(const Animation &animation) -> void {
    pose_->Reset(*model_);

    animation.IterateNodeChannels([&](uint32_t channel_id, const Animation::AnyNodeChannel &channel) -> void {
        channel.Start(*pose_, node_channel_data_[channel_id]);
    });
}

auto Model::Controller::InitializeAnimationData() -> void {
    auto animation = model_->GetAnimation(animation_);

    if (animation) {
        node_channel_data_.resize(animation->GetNodeChannelCount());
        ResetAnimationData(*animation);
    }
}

using KeyframeQuat = Model::Animation::template Keyframe<glm::fquat>;

template <AnimationPropertyType T>
inline auto Interpolate(
    Model::Animation::InterpolationMode mode, const Model::Animation::template Keyframe<T> &k0,
    const Model::Animation::template Keyframe<T> &k1, float t, T *result) -> void {
    const auto &v0 = k0.value;
    const auto &v1 = k1.value;
    const auto t0 = k0.time;
    const auto t1 = k1.time;
    float l = (t1 - t0);
    float _t = (glm::clamp(t, t0, t1) - t0) / l;

    switch (mode) {
    case Model::Animation::InterpolationMode::eLinear:
        *result = glm::mix(v0, v1, _t);
        break;
    case Model::Animation::InterpolationMode::eHermite: {
        const auto &m0 = k0.out_dy; // tangents
        const auto &m1 = k1.in_dy;

        *result = glm::hermite(v0, l * m0, v1, l * m1, _t);
        break;
    }
    case Model::Animation::InterpolationMode::eCubic:
    case Model::Animation::InterpolationMode::eStep:
        *result = v0;
        break;
    }
}

inline auto Interpolate(
    Model::Animation::InterpolationMode mode, const KeyframeQuat &k0, const KeyframeQuat &k1, float t,
    glm::fquat *result) -> void {
    const auto &v0 = k0.value;
    const auto &v1 = k1.value;
    const auto t0 = k0.time;
    const auto t1 = k1.time;
    float l = (t1 - t0);
    float _t = (glm::clamp(t, t0, t1) - t0) / l;

    switch (mode) {
    case Model::Animation::InterpolationMode::eLinear:
        *result = glm::slerp(v0, v1, _t);
        break;
    case Model::Animation::InterpolationMode::eHermite: {
        const auto &m0 = k0.out_dy; // tangents
        const auto &m1 = k1.in_dy;

        *result = glm::hermite(v0, l * m0, v1, l * m1, _t);
        break;
    }
    case Model::Animation::InterpolationMode::eCubic:
    case Model::Animation::InterpolationMode::eStep:
        *result = v0;
        break;
    }
}

template <>
auto Model::Animation::NodeChannel<Model::Animation::NodeTargetProperty::eTranslation>::Interpolate(
    const KeyframeType &k0, const KeyframeType &k1, float time, Pose::Node *node) const -> void {
    glm::fvec3 result = node->GetTranslation();
    ::render::Interpolate(interpolation_, k0, k1, time, &result);

    node->SetTranslationSilent(result);
}

template <>
auto Model::Animation::NodeChannel<Model::Animation::NodeTargetProperty::eRotation>::Interpolate(
    const KeyframeType &k0, const KeyframeType &k1, float time, Pose::Node *node) const -> void {
    glm::fquat result = node->GetRotation();
    ::render::Interpolate(interpolation_, k0, k1, time, &result);

    node->SetRotationSilent(result);
}

template <>
auto Model::Animation::NodeChannel<Model::Animation::NodeTargetProperty::eScale>::Interpolate(
    const KeyframeType &k0, const KeyframeType &k1, float time, Pose::Node *node) const -> void {
    glm::fvec3 result = node->GetScale();
    ::render::Interpolate(interpolation_, k0, k1, time, &result);

    node->SetScaleSilent(result);
}

template <>
auto Model::Animation::NodeChannel<Model::Animation::NodeTargetProperty::eColor>::Interpolate(
    const KeyframeType &k0, const KeyframeType &k1, float time, Pose::Node *node) const -> void {
    glm::fvec4 result = node->GetColor();
    ::render::Interpolate(interpolation_, k0, k1, time, &result);

    node->SetColor(result);
}

template <>
auto Model::Animation::NodeChannel<Model::Animation::NodeTargetProperty::eUvOffset>::Interpolate(
    const KeyframeType &k0, const KeyframeType &k1, float time, Pose::Node *node) const -> void {
    glm::fvec2 result = node->GetUvOffset();
    ::render::Interpolate(interpolation_, k0, k1, time, &result);

    node->SetUvOffset(result);
}

template <>
auto Model::Animation::NodeChannel<Model::Animation::NodeTargetProperty::eUvScale>::Interpolate(
    const KeyframeType &k0, const KeyframeType &k1, float time, Pose::Node *node) const -> void {
    glm::fvec2 result = node->GetUvScale();
    ::render::Interpolate(interpolation_, k0, k1, time, &result);

    node->SetUvScale(result);
}

template <>
auto Model::Animation::NodeChannel<Model::Animation::NodeTargetProperty::eAlpha>::Interpolate(
    const KeyframeType &k0, const KeyframeType &k1, float time, Pose::Node *node) const -> void {
    glm::fvec1 result = node->GetAlpha();
    ::render::Interpolate(interpolation_, k0, k1, time, &result);

    node->SetAlpha(result);
}

auto Model::Controller::UpdateAnimation(const Animation &animation) -> void {
    animation.IterateNodeChannels([&](uint32_t channel_id, const Animation::AnyNodeChannel &channel) -> void {
        channel.Apply(time_, *pose_.get(), node_channel_data_[channel_id]);
    });

    pose_->RecomputeTransformSubtree(pose_->GetRoot());
}

auto Model::CreateController(AnimationId animation) -> Controller { return Controller{this, animation}; }

} // namespace render
