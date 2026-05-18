#pragma once
#include <string>
#include <variant>

#include "render/render.hpp"
#include "common/common.hpp"

namespace render {

class Drawlist final {
private:
    struct NodeTag {};
    struct TextureTag {};
    struct VertexBufferTag {};
    struct SkinnedVertexBufferTag {};
    struct SkinTag {};
    struct MotionTag {};

public:
    template <typename Tag> class Id {
    public:
        auto index() const -> uint32_t { return index_; }

        Id(const Id &) = default;
        auto operator=(const Id &) -> Id & = default;

    private:
        explicit Id(uint32_t index) : index_{index} {}
        uint32_t index_;

        friend Drawlist;
    };

    using NodeId = Id<NodeTag>;
    using TextureId = Id<TextureTag>;
    using VertexBufferId = Id<VertexBufferTag>;
    using SkinnedVertexBufferId = Id<SkinnedVertexBufferTag>;
    using SkinId = Id<SkinTag>;
    using MotionId = Id<MotionTag>;

    class Texture final {
    public:
        ~Texture() noexcept = default;

        Texture(const Texture &) = delete;
        auto operator=(const Texture &) = delete;

        Texture(Texture &&t) noexcept = default;
        auto operator=(Texture &&t) noexcept -> Texture & = default;

        auto GetModel() const -> const Drawlist & { return *model_; }
        auto GetHandle() const -> const hal::TextureHandle & { return handle_; }

        auto GetUvOffset() const -> const glm::fvec2 & { return uv_offset_; }
        auto GetUvScale() const -> const glm::fvec2 & { return uv_scale_; }

    private:
        Texture(Drawlist *model, hal::TextureHandle handle, const glm::fvec2 &uv_offset, const glm::fvec2 &uv_scale)
            : model_{model}, handle_{std::move(handle)}, uv_offset_{uv_offset}, uv_scale_{uv_scale} {}

        Drawlist *model_ = nullptr;
        hal::TextureHandle handle_;

        glm::fvec2 uv_offset_ = glm::fvec2{0.0f, 0.0f};
        glm::fvec2 uv_scale_ = glm::fvec2{1.0f, 1.0f};

        friend Drawlist;
    };

    class VertexBuffer final {
    public:
        ~VertexBuffer() noexcept = default;

        VertexBuffer(const VertexBuffer &) = delete;
        auto operator=(const VertexBuffer &) = delete;

        VertexBuffer(VertexBuffer &&t) noexcept = default;
        auto operator=(VertexBuffer &&t) noexcept -> VertexBuffer & = default;

        auto GetModel() const -> const Drawlist & { return *model_; }
        auto GetHandle() const -> const hal::MeshHandle & { return handle_; }

    private:
        VertexBuffer(Drawlist *model, hal::MeshHandle handle) : model_{model}, handle_{std::move(handle)} {}

        Drawlist *model_ = nullptr;
        hal::MeshHandle handle_;

        friend Drawlist;
    };

    class Node final {
    public:
        Node(const Node &) = delete;
        auto operator=(const Node &) = delete;

        Node(Node &&) noexcept = default;
        auto operator=(Node &&) noexcept -> Node & = default;

        auto GetModel() const -> const Drawlist & { return *model_; }
        auto GetName() const -> std::string_view { return name_; }

    private:
        Node(Drawlist *model, std::string_view name) : model_{model}, name_{name} {}

        Drawlist *model_ = nullptr;
        std::string name_;

        friend Drawlist;
    };

    class Skin final {
    public:
        Skin(const Skin &) = delete;
        auto operator=(const Skin &) = delete;

        Skin(Skin &&) noexcept = default;
        auto operator=(Skin &&) noexcept -> Skin & = default;

        auto GetModel() const -> const Drawlist & { return *model_; }
        auto GetHandle() const -> const hal::SkinningBufferHandle { return handle_; }

    private:
        Skin(Drawlist *model, hal::SkinningBufferHandle handle) : model_{model}, handle_{handle} {}

        Drawlist *model_ = nullptr;
        hal::SkinningBufferHandle handle_;

        friend Drawlist;
    };

    class SkinnedVertexBuffer final {
    public:
        ~SkinnedVertexBuffer() noexcept = default;

        SkinnedVertexBuffer(const SkinnedVertexBuffer &) = delete;
        auto operator=(const SkinnedVertexBuffer &) = delete;

        SkinnedVertexBuffer(SkinnedVertexBuffer &&t) noexcept = default;
        auto operator=(SkinnedVertexBuffer &&t) noexcept -> SkinnedVertexBuffer & = default;

        auto GetModel() const -> const Drawlist & { return *model_; }
        auto GetHandle() const -> const hal::AnimMeshHandle & { return handle_; }

    private:
        SkinnedVertexBuffer(Drawlist *model, hal::AnimMeshHandle handle) : model_{model}, handle_{std::move(handle)} {}

        Drawlist *model_ = nullptr;
        hal::AnimMeshHandle handle_;

        friend Drawlist;
    };

    class Motion final {
    public:
        struct DrawParameters {
            std::optional<TextureId> diffuse_map = std::nullopt;
            glm::fvec2 uv_offset = glm::fvec2{0.0f, 0.0f};
            glm::fvec2 uv_scale = glm::fvec2{1.0f, 1.0f};
            glm::fvec4 color = glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f};
            glm::fmat4x4 world_matix = glm::fmat4x4{1.0f};
        };

        struct DrawCommand {
            VertexBufferId mesh;
            DrawParameters parameters;
        };

        struct SkinnedDrawCommand {
            SkinnedVertexBufferId mesh;
            SkinId skin;
            DrawParameters parameters;
        };

        using AnyDrawCommand = std::variant<DrawCommand, SkinnedDrawCommand>;

        class Frame final {
        public:
            Frame(float time) noexcept : time_{time} {}

            Frame(const Frame &) = delete;
            auto operator=(const Frame &) = delete;

            Frame(Frame &&) noexcept = default;
            auto operator=(Frame &&) noexcept -> Frame & = default;

            auto AppendCommand(AnyDrawCommand command) -> void { commands_.emplace_back(std::move(command)); }

            template <TypedForwardRange<AnyDrawCommand> Range> auto AppendCommands(Range commands) -> void {
                commands_.emplace_back(
                    commands_.end(), std::make_move_iterator(std::ranges::begin(commands), std::ranges::end(commands)));
            }

            auto GetTime() const -> float { return time_; }
            auto GetCommands() const -> std::span<const AnyDrawCommand> { return commands_; }
            auto GetNodeOffset(NodeId node) const -> const glm::fmat4x4 &;

            auto SetNodeOffset(NodeId node, const glm::fmat4x4 &offset) -> void;

        private:
            float time_;
            std::vector<AnyDrawCommand> commands_;
            std::vector<glm::fmat4x4> node_offsets_;
        };

        Motion(const Motion &) = delete;
        auto operator=(const Motion &) = delete;

        Motion(Motion &&) noexcept = default;
        auto operator=(Motion &&) noexcept -> Motion & = default;

        auto GetName() const -> std::string_view { return name_; }
        auto GetDuration() const -> float { return duration_; }
        auto GetFrames() const -> std::span<const Frame> { return frames_; }

        auto AppendFrame(Frame &&frame) -> void;

        template <TypedForwardRange<Frame> Range> auto AppendFrames(Range frames) -> void {
            const auto max_frame_time =
                std::max_element(std::ranges::begin(frames), std::ranges::end(frames), [&](const auto &frame) {
                return frame->GetTime();
            });

            frames_.emplace_back(
                frames_.end(), std::make_move_iterator(std::ranges::begin(frames)),
                std::make_move_iterator(std::ranges::end(frames)));

            duration_ = std::max(max_frame_time, duration_);
        }

        auto Sort() -> void;

    private:
        Motion(std::string_view name, float duration) : name_{name}, duration_{duration} {}

        std::string name_;
        float duration_ = 0.0f;
        std::vector<Frame> frames_;

        friend Drawlist;
    };

    class Controller final {
    public:
        ~Controller() noexcept = default;

        Controller(const Controller &) = delete;
        auto operator=(const Controller &) = delete;

        Controller(Controller &&) noexcept = default;
        auto operator=(Controller &&) noexcept -> Controller & = default;

        auto GetDevice() -> hal::IDevice & { return *device_; }
        auto GetDevice() const -> hal::IDevice & { return *device_; }

        auto GetLoop() const -> bool { return loop_; }
        auto GetTime() const -> float { return time_; }
        auto GetDuration() const -> float;
        auto GetNumFrames() const -> uint32_t;

        auto SetLoop(bool loop) -> void { loop_ = loop; }
        auto SetMotion(MotionId motion) -> void;

        auto Seek(float time) -> void;
        auto Integrate(float delta_time) -> void;
        auto Render(const glm::fmat4x4 &world_matrix) const -> void;

    private:
        auto InitMotionData(const Motion &motion) -> void;
        auto UpdateMotion(const Motion &motion) -> void;
        auto ResetMotion(const Motion &motion) -> void;

        Controller(const Drawlist *model, MotionId motion) : device_{model->device_}, model_{model}, motion_{motion} {
            auto motion_ptr = model_->GetMotion(motion);
            if (motion_ptr) {
                InitMotionData(*motion_ptr);
            }
        }

        hal::IDevice *device_ = nullptr;
        const Drawlist *model_ = nullptr;
        MotionId motion_;

        float time_ = 0.0f;
        bool loop_ = true;

        uint32_t current_frame_ = 0;
        float current_frame_time_ = 0.0f;
        float next_frame_time_ = 0.0f;

        friend Drawlist;
    };

    auto CreateController(MotionId motion) const -> Controller;

    Drawlist(hal::IDevice *device) : device_{device} {}
    ~Drawlist() noexcept = default;

    Drawlist(const Drawlist &) = delete;
    auto operator=(const Drawlist &) = delete;

    Drawlist(Drawlist &&) = delete;
    auto operator=(Drawlist &&) = delete;

    auto GetDevice() -> hal::IDevice & { return *device_; }
    auto GetDevice() const -> hal::IDevice & { return *device_; }

    auto GetVertexBuffer(VertexBufferId handle) -> VertexBuffer *;
    auto GetSkinnedVertexBuffer(SkinnedVertexBufferId handle) -> SkinnedVertexBuffer *;
    auto GetNamedNode(NodeId handle) -> Node *;
    auto GetTexture(TextureId handle) -> Texture *;
    auto GetSkin(SkinId handle) -> Skin *;
    auto GetMotion(MotionId handle) -> Motion *;

    auto GetVertexBuffer(VertexBufferId handle) const -> const VertexBuffer *;
    auto GetSkinnedVertexBuffer(SkinnedVertexBufferId handle) const -> const SkinnedVertexBuffer *;
    auto GetNamedNode(NodeId handle) const -> const Node *;
    auto GetTexture(TextureId handle) const -> const Texture *;
    auto GetSkin(SkinId handle) const -> const Skin *;
    auto GetMotion(MotionId handle) const -> const Motion *;

    auto FindNamedNode(std::string_view name) const -> std::optional<NodeId>;

    auto AddNamedNode(std::string_view name) -> std::optional<NodeId>;
    auto AddSkin(std::span<const glm::fmat4x4> skin_matrices) -> std::optional<SkinId>;

    auto AddVertexBuffer(std::span<const StaticVertex> vertices, std::span<const uint32_t> indices)
        -> std::optional<VertexBufferId>;
    auto AddSkinnedVertexBuffer(std::span<const AnimatedVertex> vertices, std::span<const uint32_t> indices)
        -> std::optional<SkinnedVertexBufferId>;
    auto AddRgbaTexture(
        uint32_t width, uint32_t height, std::span<const uint8_t> range, const glm::fvec2 &uv_offset,
        const glm::fvec2 &uv_scale) -> std::optional<TextureId>;

    template <std::invocable<Motion &> F>
    auto AddMotion(std::string_view name, float duration, F builder) -> std::optional<MotionId> {
        Motion motion{name, duration};
        builder(motion);

        motion.Sort();
        motions_.emplace_back(std::move(motion));

        return MotionId{static_cast<uint32_t>(motions_.size() - 1)};
    }

    auto MakeMotionList() const -> std::vector<MotionId>;

private:
    hal::IDevice *device_ = nullptr;

    std::vector<Node> named_nodes_;
    std::vector<Texture> textures_;
    std::vector<VertexBuffer> vertex_buffers_;
    std::vector<Skin> skins_;
    std::vector<SkinnedVertexBuffer> skinned_vertex_buffers_;
    std::vector<Motion> motions_;
};

} // namespace render