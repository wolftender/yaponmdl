#pragma once
#include <algorithm>
#include <optional>
#include <string>
#include <vector>
#include <variant>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "render/render.hpp"

namespace render {

template <typename T>
concept AnimationPropertyType = std::convertible_to<T, glm::fvec3> || std::convertible_to<T, glm::fquat>;

class Model final {
    struct MeshTag {};
    struct NodeTag {};
    struct TextureTag {};
    struct MaterialTag {};
    struct SkinTag {};
    struct AnimatedMeshTag {};
    struct AnimationTag {};

public:
    template <typename Tag> class Id {
    public:
        auto index() const -> uint32_t { return index_; }

        Id(const Id &) = default;
        auto operator=(const Id &) -> Id & = default;

    private:
        explicit Id(uint32_t index) : index_{index} {}
        uint32_t index_;

        friend class Node;
        friend class Model;
        friend class Pose;
    };

    using MeshId = Id<MeshTag>;
    using NodeId = Id<NodeTag>;
    using TextureId = Id<TextureTag>;
    using MaterialId = Id<MaterialTag>;
    using SkinId = Id<SkinTag>;
    using AnimatedMeshId = Id<AnimatedMeshTag>;
    using AnimationId = Id<AnimationTag>;

    class Texture final {
    public:
        ~Texture() noexcept = default;

        Texture(const Texture &) = delete;
        auto operator=(const Texture &) = delete;

        Texture(Texture &&t) noexcept = default;
        auto operator=(Texture &&t) noexcept -> Texture & = default;

        auto GetModel() const -> const Model & { return *model_; }
        auto GetHandle() const -> const hal::TextureHandle & { return handle_; }

    private:
        Texture(Model *model, hal::TextureHandle handle) : model_{model}, handle_{std::move(handle)} {}

        Model *model_ = nullptr;
        hal::TextureHandle handle_;

        friend class Model;
    };

    class Mesh final {
    public:
        ~Mesh() noexcept = default;

        Mesh(const Mesh &) = delete;
        auto operator=(const Mesh &) = delete;

        Mesh(Mesh &&t) noexcept = default;
        auto operator=(Mesh &&t) noexcept -> Mesh & = default;

        auto GetModel() const -> const Model & { return *model_; }
        auto GetHandle() const -> const hal::MeshHandle & { return handle_; }
        auto GetMaterial() const -> MaterialId { return material_; }

    private:
        Mesh(Model *model, hal::MeshHandle handle, MaterialId material)
            : model_{model}, handle_{std::move(handle)}, material_{material} {}

        Model *model_ = nullptr;
        hal::MeshHandle handle_;
        MaterialId material_;

        friend class Model;
    };

    class Skin final {
    public:
        struct SkinNode {
            NodeId node;
            glm::fmat4x4 inverse_bind;
        };

        Skin() = default;
        Skin(std::span<const SkinNode> nodes) : nodes_{std::ranges::begin(nodes), std::ranges::end(nodes)} {}

        auto AddNodeRef(NodeId node, const glm::fmat4x4 &inverse_bind) -> void {
            nodes_.push_back({node, inverse_bind});
        }

        auto AddNodeRef(SkinNode node) -> void { nodes_.emplace_back(node); }
        auto GetNodes() const -> const std::vector<SkinNode> & { return nodes_; }

    private:
        std::vector<SkinNode> nodes_;
    };

    class AnimatedMesh final {
    public:
        ~AnimatedMesh() noexcept = default;

        AnimatedMesh(const AnimatedMesh &) = delete;
        auto operator=(const AnimatedMesh &) = delete;

        AnimatedMesh(AnimatedMesh &&t) noexcept = default;
        auto operator=(AnimatedMesh &&t) noexcept -> AnimatedMesh & = default;

        auto GetModel() const -> const Model & { return *model_; }
        auto GetHandle() const -> const hal::AnimMeshHandle & { return handle_; }
        auto GetMaterial() const -> MaterialId { return material_; }
        auto GetSkin() const -> SkinId { return skin_; }

    private:
        AnimatedMesh(Model *model, hal::AnimMeshHandle handle, MaterialId material, SkinId skin)
            : model_{model}, handle_{std::move(handle)}, material_{material}, skin_{std::move(skin)} {}

        Model *model_ = nullptr;
        hal::AnimMeshHandle handle_;
        MaterialId material_;
        SkinId skin_;

        friend class Model;
    };

    class Node final {
    public:
        Node(const Node &) = delete;
        auto operator=(const Node &) = delete;

        Node(Node &&) noexcept = default;
        auto operator=(Node &&) noexcept -> Node & = default;

        auto GetModel() const -> const Model & { return *model_; }
        auto GetSelf() const -> NodeId { return self_; }
        auto GetParent() const -> const std::optional<NodeId> { return parent_; }
        auto GetTranslation() const -> const glm::fvec3 & { return translation_; }
        auto GetScale() const -> const glm::fvec3 & { return scale_; }
        auto GetRotation() const -> const glm::fquat & { return rotation_; }
        auto GetChildren() const -> const std::vector<NodeId> & { return children_; }
        auto GetMeshes() const -> const std::vector<MeshId> & { return meshes_; }
        auto GetAnimMeshes() const -> const std::vector<AnimatedMeshId> & { return animated_meshes_; }

        auto SetTranslation(const glm::fvec3 &translation) -> void { translation_ = translation; }
        auto SetScale(const glm::fvec3 &scale) -> void { scale_ = scale; }
        auto SetRotation(const glm::fquat &rotation) -> void { rotation_ = rotation; }

        auto AddMesh(MeshId id) -> void;
        auto AddAnimMesh(AnimatedMeshId id) -> void;

    private:
        Node(
            Model *model, NodeId self, std::string_view name, std::optional<NodeId> parent,
            const glm::fvec3 &translation, const glm::fvec3 &scale, const glm::fquat &rotation)
            : model_{model}, self_{self}, name_{name}, parent_{parent}, translation_{translation}, scale_{scale},
              rotation_{rotation} {}

        Model *model_ = nullptr;
        NodeId self_;
        std::string name_;
        std::optional<NodeId> parent_;
        glm::fvec3 translation_;
        glm::fvec3 scale_;
        glm::fquat rotation_;
        std::vector<MeshId> meshes_;
        std::vector<AnimatedMeshId> animated_meshes_;
        std::vector<NodeId> children_;

        friend class Model;
    };

    class Material final {
    public:
        struct Description {
            std::optional<TextureId> diffuse;
            std::optional<TextureId> normal;
            std::optional<TextureId> specular;
        };

        Material(const Material &) = delete;
        auto operator=(const Material &) = delete;

        Material(Material &&) noexcept = default;
        auto operator=(Material &&) noexcept -> Material & = default;

        auto GetModel() const -> const Model & { return *model_; }
        auto GetDiffuse() const -> std::optional<TextureId> { return diffuse_; }
        auto GetNormal() const -> std::optional<TextureId> { return normal_; }
        auto GetSpecular() const -> std::optional<TextureId> { return specular_; }

    private:
        Material(
            Model *model, std::optional<TextureId> diffuse, std::optional<TextureId> normal,
            std::optional<TextureId> specular)
            : model_{model}, diffuse_{diffuse}, normal_{normal}, specular_{specular} {}

        Model *model_ = nullptr;
        std::optional<TextureId> diffuse_;
        std::optional<TextureId> normal_;
        std::optional<TextureId> specular_;

        friend class Model;
    };

    class Animation final {
    public:
        enum class InterpolationMode {
            eStep,
            eLinear,
            eCubic,
        };

        enum class TargetProperty {
            eRotation,
            eTranslation,
            eScale,
        };

        template <TargetProperty Prop> struct PropertyDataType;

        template <> struct PropertyDataType<TargetProperty::eRotation> {
            using Type = glm::fquat;
        };

        template <> struct PropertyDataType<TargetProperty::eTranslation> {
            using Type = glm::fvec3;
        };

        template <> struct PropertyDataType<TargetProperty::eScale> {
            using Type = glm::fvec3;
        };

        template <AnimationPropertyType T> struct Keyframe final {
            T value;
            float time;
        };

        template <TargetProperty Prop> class Channel final {
        public:
            using PropertyType = typename PropertyDataType<Prop>::Type;
            using KeyframeType = Keyframe<PropertyType>;

            explicit Channel(NodeId node) : node_{node}, interpolation_{InterpolationMode::eLinear} {}

            ~Channel() noexcept = default;

            Channel(const Channel &) = delete;
            auto operator=(const Channel &) = delete;

            Channel(Channel &&) noexcept = default;
            auto operator=(Channel &&) noexcept -> Channel & = default;

            auto GetKeyframes() const -> const std::vector<KeyframeType> & { return keyframes_; }
            auto GetInterpolation() const -> InterpolationMode { return interpolation_; }
            auto GetNode() const -> NodeId { return node_; }

            auto GetKeyframes() -> std::vector<KeyframeType> & { return keyframes_; }
            auto SetInterpolation(InterpolationMode mode) { interpolation_ = mode; }

            auto sort() -> void {
                std::sort(
                    keyframes_.begin(), keyframes_.end(), [](const auto &a, const auto &b) { return a.time < b.time; });
            }

        private:
            std::vector<KeyframeType> keyframes_;

            NodeId node_;
            InterpolationMode interpolation_;
        };

        using TranslationChannel = Channel<TargetProperty::eTranslation>;
        using RotationChannel = Channel<TargetProperty::eRotation>;
        using ScaleChannel = Channel<TargetProperty::eScale>;

        using AnyChannel = std::variant<RotationChannel, TranslationChannel, ScaleChannel>;

        explicit Animation(std::string_view name) : duration_{0.0f}, name_{name} {}

        Animation(const Animation &) = delete;
        auto operator=(const Animation &) = delete;

        Animation(Animation &&) noexcept = default;
        auto operator=(Animation &&) noexcept -> Animation & = default;

        auto GetDuration() const -> float { return duration_; }
        auto GetName() const -> const std::string & { return name_; }

        auto GetTranslationChanCount() const -> uint32_t { return translation_channels_.size(); }
        auto GetRotationChanCount() const -> uint32_t { return rotation_channels_.size(); }
        auto GetScaleChanCount() const -> uint32_t { return scale_channels_.size(); }

        template <TargetProperty Prop> auto AppendChannel(Channel<Prop> &&channel) -> void {
            const auto last_keyframe_time = !channel.GetKeyframes().empty() ? channel.GetKeyframes().back().time : 0.0f;
            duration_ = std::max(duration_, last_keyframe_time);

            if constexpr (Prop == TargetProperty::eTranslation) {
                translation_channels_.emplace_back(std::move(channel));
            } else if constexpr (Prop == TargetProperty::eRotation) {
                rotation_channels_.emplace_back(std::move(channel));
            } else if constexpr (Prop == TargetProperty::eScale) {
                scale_channels_.emplace_back(std::move(channel));
            } else {
                static_assert(false, "unsupported channel type");
            }
        }

        template <TargetProperty Prop, std::invocable<uint32_t, const Channel<Prop> &> F>
        auto IterateChannels(F consumer) const -> void {
            auto &channels = [this]() -> const auto & {
                if constexpr (Prop == TargetProperty::eTranslation) {
                    return translation_channels_;
                } else if constexpr (Prop == TargetProperty::eRotation) {
                    return rotation_channels_;
                } else if constexpr (Prop == TargetProperty::eScale) {
                    return scale_channels_;
                }
            }();

            for (uint32_t channel_id = 0; channel_id < channels.size(); ++channel_id) {
                consumer(channel_id, channels[channel_id]);
            }
        }

    private:
        std::vector<TranslationChannel> translation_channels_;
        std::vector<RotationChannel> rotation_channels_;
        std::vector<ScaleChannel> scale_channels_;

        float duration_;
        std::string name_;
    };

    class Pose final {
    public:
        class Node final {
        public:
            Node(const Node &) = delete;
            auto operator=(const Node &) = delete;

            Node(Node &&) noexcept = default;
            auto operator=(Node &&) noexcept -> Node & = default;

            auto GetPose() -> Pose & { return *pose_; }
            auto GetPose() const -> const Pose & { return *pose_; }

            auto GetSelf() const -> NodeId { return self_; }
            auto GetTransform() const -> const glm::fmat4x4 & { return transform_; }

            auto SetTranslation(const glm::fvec3 &translation) -> void;
            auto SetScale(const glm::fvec3 &scale) -> void;
            auto SetRotation(const glm::fquat &rotation) -> void;

            auto SetTranslationSilent(const glm::fvec3 &translation) -> void;
            auto SetScaleSilent(const glm::fvec3 &scale) -> void;
            auto SetRotationSilent(const glm::fquat &rotation) -> void;

            auto GetTranslation() const -> const glm::fvec3 & { return translation_; }
            auto GetScale() const -> const glm::fvec3 & { return scale_; }
            auto GetRotation() const -> const glm::fquat & { return rotation_; }

            auto SetTransform(const glm::fvec3 &translation, const glm::fvec3 &scale, const glm::fquat &rotation)
                -> void;

        private:
            Node(Pose *pose, NodeId node) : pose_{pose}, self_{node} {};

            Pose *pose_;
            NodeId self_;
            std::optional<NodeId> parent_;

            glm::fvec3 translation_;
            glm::fvec3 scale_;
            glm::fquat rotation_;
            glm::fmat4x4 transform_;

            std::vector<NodeId> children_;

            friend class Model;
            friend class Pose;
        };

        ~Pose() noexcept;

        Pose(const Pose &) = delete;
        auto operator=(const Pose &) = delete;

        Pose(Pose &&) noexcept = delete;
        auto operator=(Pose &&) noexcept -> Pose & = delete;

        auto GetDevice() -> hal::IDevice & { return *device_; }
        auto GetDevice() const -> hal::IDevice & { return *device_; }

        auto GetRoot() const -> NodeId { return NodeId{0ul}; }
        auto GetNode(NodeId handle) -> Node *;
        auto GetNode(NodeId handle) const -> const Node *;

        template <std::invocable<NodeId, glm::fvec3 &, glm::fvec3 &, glm::fquat &> Curve>
        auto ApplyCurve(Curve curve) -> void {
            for (auto &node : nodes_) {
                curve(node.self_, node.translation_, node.scale_, node.rotation_);
            }

            RecomputeTransformSubtree(GetRoot());
        }

    private:
        static auto FromModel(const Model &model) -> std::unique_ptr<Pose>;

        Pose() = default;

        auto RecomputeTransformSubtree(NodeId root) -> void;

        hal::IDevice *device_;

        std::vector<Node> nodes_;
        std::vector<hal::SkinningBufferHandle> skinning_buffers_;

        friend class Model;
        friend class Controller;
    };

    Model(hal::IDevice *Device);
    ~Model() noexcept = default;

    Model(const Model &) = delete;
    auto operator=(const Model &) = delete;

    Model(Model &&) noexcept = delete;
    auto operator=(Model &&) noexcept = delete;

    auto GetDevice() -> hal::IDevice & { return *device_; }
    auto GetDevice() const -> const hal::IDevice & { return *device_; }

    auto CreatePose() const -> std::unique_ptr<Pose>;
    auto GetRoot() const -> NodeId { return NodeId{0ul}; }

    auto AddMesh(std::span<const StaticVertex> vertices, std::span<const uint32_t> indices, MaterialId material)
        -> std::optional<MeshId> {
        const auto vtx_ptr = std::ranges::data(vertices);
        const auto idx_ptr = std::ranges::data(indices);

        const auto vtx_len = std::ranges::size(vertices);
        const auto idx_len = std::ranges::size(indices);

        return AddMeshImpl({vtx_ptr, vtx_len}, {idx_ptr, idx_len}, material);
    }

    auto AddAnimatedMesh(
        std::span<const AnimatedVertex> vertices, std::span<const uint32_t> indices, MaterialId material, SkinId skin)
        -> std::optional<AnimatedMeshId> {
        const auto vtx_ptr = std::ranges::data(vertices);
        const auto idx_ptr = std::ranges::data(indices);

        const auto vtx_len = std::ranges::size(vertices);
        const auto idx_len = std::ranges::size(indices);

        return AddAnimMeshImpl({vtx_ptr, vtx_len}, {idx_ptr, idx_len}, material, skin);
    }

    auto AddRgbaTexture(uint32_t width, uint32_t height, std::span<const uint8_t> range) -> std::optional<TextureId> {
        const auto data_ptr = std::ranges::data(range);
        const auto data_len = std::ranges::size(range);

        return AddRgbaTextureImpl(width, height, {data_ptr, data_len});
    }

    auto AddSkin(Skin &&skin) -> std::optional<SkinId>;

    auto AddMaterial(const Material::Description &desc) -> std::optional<MaterialId>;
    auto AddNode(NodeId parent, std::string_view name) -> std::optional<NodeId>;
    auto AddAnimation(Animation &&animation) -> std::optional<AnimationId>;

    auto GetMesh(MeshId handle) -> Mesh *;
    auto GetAnimMesh(AnimatedMeshId handle) -> AnimatedMesh *;
    auto GetNode(NodeId handle) -> Node *;
    auto GetTexture(TextureId handle) -> Texture *;
    auto GetMaterial(MaterialId handle) -> Material *;
    auto GetAnimation(AnimationId handle) -> Animation *;

    auto GetMesh(MeshId handle) const -> const Mesh *;
    auto GetAnimMesh(AnimatedMeshId handle) const -> const AnimatedMesh *;
    auto GetNode(NodeId handle) const -> const Node *;
    auto GetTexture(TextureId handle) const -> const Texture *;
    auto GetMaterial(MaterialId handle) const -> const Material *;
    auto GetAnimation(AnimationId handle) const -> const Animation *;

    std::vector<AnimationId> MakeAnimationList() const {
        std::vector<AnimationId> animations;
        for (uint32_t anim_id = 0; anim_id < animations_.size(); ++anim_id) {
            animations.push_back(AnimationId{anim_id});
        }

        return animations;
    }

    template <std::invocable<NodeId, const Node &> F> auto IterateNodes(F consumer) const -> void {
        for (uint32_t id = 0; id < nodes_.size(); ++id) {
            consumer(NodeId{id}, nodes_[id]);
        }
    }

    template <std::invocable<NodeId, const Node &> F> auto IterateMeshNodes(F consumer) const -> void {
        for (const auto &id : mesh_nodes_) {
            consumer(id, nodes_[id.index()]);
        }
    }

    auto Render(Pose &pose, const glm::fmat4x4 &world) const -> void;

    class Controller final {
    private:
        struct ChannelData {
            uint32_t prev_keyframe;
            uint32_t next_keyframe;

            float prev_keyframe_time;
            float next_keyframe_time;
        };

        Controller(const Model *model, AnimationId animation)
            : model_{model}, animation_{animation}, time_{0.0f}, loop_{true} {
            pose_ = model->CreatePose();
            InitializeAnimationData();
        }

        using TranslationChannel = Animation::TranslationChannel;
        using RotationChhannel = Animation::RotationChannel;
        using ScaleChannel = Animation::ScaleChannel;

    public:
        Controller(const Controller &) = delete;
        auto operator=(const Controller &) -> Controller & = delete;

        Controller(Controller &&) noexcept = default;
        auto operator=(Controller &&) noexcept -> Controller & = default;

        ~Controller() = default;

        auto GetAnimation() const -> AnimationId { return animation_; }
        auto GetLoop() const -> bool { return loop_; }
        auto GetTime() const -> float { return time_; }

        auto GetPose() -> Pose & { return *pose_.get(); }
        auto GetPose() const -> const Pose & { return *pose_.get(); }

        auto SetLoop(bool loop) -> void { loop_ = loop; }

        auto SetAnimation(AnimationId id) -> void;
        auto Integrate(float delta_time) -> void;
        auto Seek(float time) -> void;

    private:
        template <Animation::TargetProperty Prop>
        auto ResetChannelData(
            std::vector<ChannelData> &channel_data, uint32_t channel_id, const Animation::Channel<Prop> &channel)
            -> void {
            const auto &keyframes = channel.GetKeyframes();

            if (keyframes.size() == 0) { // invalid channel dont initialize?
                return;
            }

            if (keyframes.size() == 1) { // one keyframe, simply add it
                channel_data[channel_id].prev_keyframe = 0;
                channel_data[channel_id].next_keyframe = 0;
                channel_data[channel_id].prev_keyframe_time = 0.0f;
                channel_data[channel_id].next_keyframe_time = 99999.0f;
                return;
            }

            // otherwise pick the first keyframe and the second keyframe
            channel_data[channel_id].prev_keyframe = 0;
            channel_data[channel_id].next_keyframe = 1;
            channel_data[channel_id].prev_keyframe_time = keyframes[0].time;
            channel_data[channel_id].next_keyframe_time = keyframes[1].time;
        }

        auto ResetAnimationData(const Animation &animation) -> void;
        auto InitializeAnimationData() -> void;

        template <
            Animation::TargetProperty Prop,
            std::invocable<
                const typename Animation::Channel<Prop>::KeyframeType &,
                const typename Animation::Channel<Prop>::KeyframeType &, float, Pose::Node &>
                F>
        auto UpdateAnimationChannel(
            std::vector<ChannelData> &channel_data_list, uint32_t channel_id, const Animation::Channel<Prop> &channel,
            F interpolate) -> void {
            const auto &keyframes = channel.GetKeyframes();
            auto &channel_data = channel_data_list[channel_id];

            if (time_ > channel_data.next_keyframe_time) {
                const auto num_keyframes = keyframes.size();
                do {
                    channel_data.prev_keyframe++;
                    channel_data.next_keyframe++;

                    channel_data.prev_keyframe_time = keyframes[channel_data.prev_keyframe].time;
                    channel_data.next_keyframe_time = keyframes[channel_data.next_keyframe].time;
                } while ((channel_data.next_keyframe < num_keyframes - 1) &&
                         (keyframes[channel_data.next_keyframe].time < time_));
            }

            const auto &prev_keyframe = keyframes[channel_data.prev_keyframe];
            const auto &next_keyframe = keyframes[channel_data.next_keyframe];

            auto node = pose_->GetNode(channel.GetNode());
            if (node) {
                interpolate(prev_keyframe, next_keyframe, time_, *node);
            }
        }

        auto UpdateAnimationChannel(uint32_t channel_id, const TranslationChannel &channel) -> void;
        auto UpdateAnimationChannel(uint32_t channel_id, const RotationChhannel &channel) -> void;
        auto UpdateAnimationChannel(uint32_t channel_id, const ScaleChannel &channel) -> void;
        auto UpdateAnimation(const Animation &animation) -> void;

        const Model *model_;

        AnimationId animation_;
        std::unique_ptr<Pose> pose_;

        float time_;
        bool loop_;

        std::vector<ChannelData> translation_data_;
        std::vector<ChannelData> rotation_data_;
        std::vector<ChannelData> scale_data_;

        friend class Model;
    };

    auto CreateController(AnimationId animation) -> Controller;

private:
    auto AddMeshImpl(std::span<const StaticVertex> vertices, std::span<const uint32_t> indices, MaterialId material)
        -> std::optional<MeshId>;
    auto AddAnimMeshImpl(
        std::span<const AnimatedVertex> vertices, std::span<const uint32_t> indices, MaterialId material, SkinId skin)
        -> std::optional<AnimatedMeshId>;
    auto AddRgbaTextureImpl(uint32_t width, uint32_t height, std::span<const uint8_t> data) -> std::optional<TextureId>;

    hal::IDevice *device_;

    std::vector<Mesh> meshes_;
    std::vector<Node> nodes_;
    std::vector<Texture> textures_;
    std::vector<Material> materials_;
    std::vector<Skin> skins_;
    std::vector<AnimatedMesh> animated_meshes_;
    std::vector<NodeId> mesh_nodes_;
    std::vector<Animation> animations_;

    friend class Node;
};

} // namespace render
