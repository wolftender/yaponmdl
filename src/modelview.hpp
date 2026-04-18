#pragma once
#include <chrono>

#include "glview.hpp"

#include "render/camera.hpp"
#include "render/model.hpp"
#include "render/device.hpp"
#include "render/text.hpp"

wxDECLARE_EVENT(MODEL_VIEWER_LOADED_MODEL, wxCommandEvent);

class ModelViewer : public GLView {
public:
    class ILoader {
    public:
        virtual ~ILoader() noexcept = default;
        virtual auto Load(render::hal::IDevice &device) const -> std::unique_ptr<render::Model> = 0;
    };

    class ICameraController {
    public:
        virtual ~ICameraController() noexcept = default;

        virtual auto GetCamera() const -> const render::hal::ICamera & = 0;
        virtual auto OnUpdateSize([[maybe_unused]] float width, [[maybe_unused]] float height) -> void {}
        virtual auto OnMouseMotion([[maybe_unused]] wxMouseEvent &event) -> void {}
        virtual auto OnMouseScroll([[maybe_unused]] wxMouseEvent &event) -> void {}
    };

    class AzimuthCameraController : public ICameraController {
    public:
        AzimuthCameraController();

        auto GetCamera() const -> const render::hal::ICamera & override { return camera_; }

        auto OnUpdateSize(float width, float height) -> void override;
        auto OnMouseMotion(wxMouseEvent &event) -> void override;
        auto OnMouseScroll(wxMouseEvent &event) -> void override;

    private:
        float zoom_ = 1.0f;
        std::optional<glm::ivec2> prev_mouse_pos_ = std::nullopt;
        render::AzimuthCamera camera_;
    };

    class OrthoCameraController : public ICameraController {
    public:
        OrthoCameraController();

        auto GetCamera() const -> const render::hal::ICamera & override { return camera_; }

        auto OnUpdateSize(float width, float height) -> void override;
        auto OnMouseMotion(wxMouseEvent &event) -> void override;
        auto OnMouseScroll(wxMouseEvent &event) -> void override;

    private:
        float zoom_ = 1.0f;
        glm::fvec2 size_;
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
    auto GetModel() const -> const render::Model * { return model_.get(); }
    auto GetAnimationList() const -> std::vector<std::string>;

    auto GetAnimationIndex() const -> uint32_t { return anim_counter_; }
    auto SetAnimationIndex(uint32_t index) -> void;

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
    std::unique_ptr<render::Model> model_;
    std::optional<render::Model::Controller> controller_;
    std::unique_ptr<render::Model::Pose> default_pose_;

    State state_ = ModelViewer::State::eIdle;
};
