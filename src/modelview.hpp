#pragma once
#include <chrono>

#include "glview.hpp"

#include "render/camera.hpp"
#include "render/model.hpp"
#include "render/drawlist.hpp"
#include "render/device.hpp"
#include "render/text.hpp"

class ModelViewerFrameEvent;

wxDECLARE_EVENT(MODEL_VIEWER_LOADED_MODEL, wxCommandEvent);
wxDECLARE_EVENT(MODEL_VIEWER_FRAME, ModelViewerFrameEvent);

class ModelViewerFrameEvent : public wxCommandEvent {
public:
    ModelViewerFrameEvent(wxEventType event_type = MODEL_VIEWER_FRAME, int id = 0) : wxCommandEvent{event_type, id} {}
    ModelViewerFrameEvent(const ModelViewerFrameEvent &event) : wxCommandEvent{event} {
        delta_time_ = event.delta_time_;
    }

    auto Clone() const -> wxEvent * { return new ModelViewerFrameEvent(*this); }

    auto GetDeltaTime() const -> float { return delta_time_; }
    auto SetDeltaTime(float delta_time) -> void { delta_time_ = delta_time; }

private:
    float delta_time_ = 0.0f;
};

class ModelViewer : public GLView {
public:
    class IModelProxy {
    public:
        virtual ~IModelProxy() noexcept = default;

        virtual auto HasAnimations() const -> bool = 0;

        virtual auto GetLoop() const -> bool = 0;
        virtual auto GetPaused() const -> bool = 0;
        virtual auto GetAnimationDuration() const -> float = 0;
        virtual auto GetAnimationTime() const -> float = 0;
        virtual auto GetAnimationList() const -> std::span<const std::string> = 0;

        virtual auto SetPaused(bool paused) -> void = 0;
        virtual auto SetLoop(bool loop) -> void = 0;
        virtual auto Seek(float time) -> void = 0;
        virtual auto SetAnimationIndex(uint32_t index) -> void = 0;

        virtual auto Integrate(float delta_time) -> void = 0;
        virtual auto Render(const glm::fmat4x4 &world_matrix) -> void = 0;
    };

    class ILoader {
    public:
        virtual ~ILoader() noexcept = default;
        virtual auto Load(render::hal::IDevice &device) const -> std::unique_ptr<IModelProxy> = 0;
    };

    class ModelProxyModel : public IModelProxy {
    public:
        ModelProxyModel(std::unique_ptr<render::Model> model);

        ModelProxyModel(const ModelProxyModel &) = delete;
        auto operator=(ModelProxyModel &) = delete;

        ModelProxyModel(ModelProxyModel &&) noexcept = default;
        auto operator=(ModelProxyModel &&) noexcept -> ModelProxyModel & = default;

        auto HasAnimations() const -> bool override;

        auto GetLoop() const -> bool override;
        auto GetPaused() const -> bool override;
        auto GetAnimationDuration() const -> float override;
        auto GetAnimationTime() const -> float override;
        auto GetAnimationList() const -> std::span<const std::string> override;

        auto SetPaused(bool paused) -> void override;
        auto SetLoop(bool loop) -> void override;
        auto Seek(float time) -> void override;
        auto SetAnimationIndex(uint32_t index) -> void override;

        auto Integrate(float delta_time) -> void override;
        auto Render(const glm::fmat4x4 &world_matrix) -> void override;

    private:
        bool paused_ = false;

        std::optional<render::Model::Controller> controller_;
        std::unique_ptr<render::Model::Pose> bind_pose_;
        std::unique_ptr<render::Model> model_;

        std::vector<std::string> animation_names_;
        std::vector<render::Model::AnimationId> animations_;
    };

    class ModelProxyDrawlist : public IModelProxy {
    public:
        ModelProxyDrawlist(std::unique_ptr<render::Drawlist> drawlist);

        ModelProxyDrawlist(const ModelProxyDrawlist &) = delete;
        auto operator=(ModelProxyDrawlist &) = delete;

        ModelProxyDrawlist(ModelProxyDrawlist &&) noexcept = default;
        auto operator=(ModelProxyDrawlist &&) noexcept -> ModelProxyDrawlist & = default;

        auto HasAnimations() const -> bool override;

        auto GetLoop() const -> bool override;
        auto GetPaused() const -> bool override;
        auto GetAnimationDuration() const -> float override;
        auto GetAnimationTime() const -> float override;
        auto GetAnimationList() const -> std::span<const std::string> override;

        auto SetPaused(bool paused) -> void override;
        auto SetLoop(bool loop) -> void override;
        auto Seek(float time) -> void override;
        auto SetAnimationIndex(uint32_t index) -> void override;

        auto Integrate(float delta_time) -> void override;
        auto Render(const glm::fmat4x4 &world_matrix) -> void override;

    private:
        bool paused_ = false;

        std::optional<render::Drawlist::Controller> controller_;
        std::unique_ptr<render::Drawlist> drawlist_;

        std::vector<std::string> animation_names_;
        std::vector<render::Drawlist::MotionId> animations_;
    };

    class ICameraController {
    public:
        virtual ~ICameraController() noexcept = default;

        virtual auto ZoomOut() -> void = 0;
        virtual auto ZoomIn() -> void = 0;
        virtual auto ResetView() -> void = 0;

        virtual auto GetCamera() const -> const render::hal::ICamera & = 0;
        virtual auto OnUpdateSize([[maybe_unused]] float width, [[maybe_unused]] float height) -> void {}
        virtual auto OnMouseMotion([[maybe_unused]] wxMouseEvent &event) -> void {}
        virtual auto OnMouseScroll([[maybe_unused]] wxMouseEvent &event) -> void {}
    };

    class AzimuthCameraController : public ICameraController {
    public:
        AzimuthCameraController();

        auto GetCamera() const -> const render::hal::ICamera & override { return camera_; }

        auto ZoomOut() -> void override;
        auto ZoomIn() -> void override;
        auto ResetView() -> void override;

        auto OnUpdateSize(float width, float height) -> void override;
        auto OnMouseMotion(wxMouseEvent &event) -> void override;
        auto OnMouseScroll(wxMouseEvent &event) -> void override;

    private:
        auto SetCameraParameters() -> void;

        float zoom_ = 1.0f;
        std::optional<glm::ivec2> prev_mouse_pos_ = std::nullopt;
        render::AzimuthCamera camera_;
    };

    class OrthoCameraController : public ICameraController {
    public:
        OrthoCameraController();

        auto GetCamera() const -> const render::hal::ICamera & override { return camera_; }

        auto ZoomOut() -> void override;
        auto ZoomIn() -> void override;
        auto ResetView() -> void override;

        auto OnUpdateSize(float width, float height) -> void override;
        auto OnMouseMotion(wxMouseEvent &event) -> void override;
        auto OnMouseScroll(wxMouseEvent &event) -> void override;

    private:
        auto SetCameraParameters() -> void;

        float zoom_ = 1.0f;
        glm::fvec2 size_;
        glm::fvec2 center_;
        std::optional<glm::ivec2> prev_mouse_pos_ = std::nullopt;
        render::OrthoCamera camera_;
    };

    static auto MakeAzimuthCamera() -> std::unique_ptr<ICameraController> {
        return std::make_unique<AzimuthCameraController>();
    }

    static auto MakeOrthoCamera() -> std::unique_ptr<ICameraController> {
        return std::make_unique<OrthoCameraController>();
    }

    ModelViewer(
        wxWindow *parent, const wxGLAttributes &attributes, std::unique_ptr<ILoader> loader,
        std::unique_ptr<ICameraController> camera_controller = MakeAzimuthCamera());

    auto GetFps() const -> uint32_t { return fps_; }
    auto GetModel() const -> const IModelProxy * { return model_proxy_.get(); }
    auto GetAnimationList() const -> std::span<const std::string>;

    auto GetAnimationIndex() const -> uint32_t { return anim_counter_; }
    auto SetAnimationIndex(uint32_t index) -> void;

    auto ZoomIn() -> void;
    auto ZoomOut() -> void;
    auto ResetView() -> void;

    auto GetCurrentAnimationTime() const -> float;
    auto GetCurrentAnimationDuration() const -> float;

    auto IsCurrentAnimationPaused() const -> bool;
    auto SetCurrentAnimationPaused(bool paused) -> void;

    auto SeekCurrentAnimation(float time) -> void;

protected:
    auto OnInitializeGL() -> void override;
    auto OnRender() -> void override;

    auto OnIdle(wxIdleEvent &event) -> void;
    auto OnSize(wxSizeEvent &event) -> void;
    auto OnMouseScroll(wxMouseEvent &event) -> void;

    auto OnMouseMotion(wxMouseEvent &event) -> void;

private:
    class AnimationTimer : public wxTimer {
    public:
        AnimationTimer(ModelViewer *viewer) : viewer_{viewer} {}

        auto Notify() -> void { viewer_->Refresh(); }
        auto Begin() -> void { wxTimer::Start(10); }

    private:
        ModelViewer *viewer_ = nullptr;
    };

    enum class State {
        eIdle,
        eGraphicsError,
        eInvalidModel,
        eInvalidTexture,
        eReady,
    };

    auto RefreshText() -> void;

    std::unique_ptr<ILoader> model_loader_ = nullptr;

    uint32_t frame_counter_ = 0;
    uint32_t anim_counter_ = 0;
    uint32_t fps_ = 0;
    float frame_timer_ = 0.0f;

    std::unique_ptr<ICameraController> camera_controller_;

    render::FontContext font_context_;
    std::optional<gl::ShaderProgram> text_shader_;
    std::optional<render::FontContext::TextObject> test_text_;

    std::optional<glm::uvec2> pending_resize_ = std::nullopt;
    std::unique_ptr<AnimationTimer> animation_timer_ = nullptr;
    std::chrono::steady_clock::time_point last_frame_;

    std::unique_ptr<render::hal::RenderDeviceOpenGL40> device_;
    std::unique_ptr<IModelProxy> model_proxy_;

    State state_ = ModelViewer::State::eIdle;
};
