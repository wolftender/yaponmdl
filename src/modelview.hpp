#pragma once
#include <chrono>

#include "glview.hpp"

#include "render/camera.hpp"
#include "render/model.hpp"
#include "render/device.hpp"
#include "render/text.hpp"

class ModelViewer : public GLView {
public:
    ModelViewer(wxWindow *parent, const wxGLAttributes &attributes, std::span<const uint8_t> gmo_buffer);

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

    uint32_t frame_counter_ = 0;
    uint32_t anim_counter_ = 0;
    uint32_t fps_ = 0;
    float frame_timer_ = 0.0f;

    render::FontContext font_context_;
    std::optional<gl::ShaderProgram> text_shader_;
    std::optional<render::FontContext::TextObject> test_text_;

    float zoom_ = 1.0f;
    std::unique_ptr<AnimationTimer> animation_timer_ = nullptr;
    std::chrono::steady_clock::time_point last_frame_;

    std::optional<glm::ivec2> prev_mouse_pos_ = std::nullopt;
    std::optional<glm::uvec2> pending_resize_ = std::nullopt;

    std::span<const uint8_t> gmo_buffer_;

    render::AzimuthCamera camera_;
    std::unique_ptr<render::hal::RenderDeviceOpenGL40> device_;
    std::unique_ptr<render::Model> model_;
    std::optional<render::Model::Controller> controller_;

    State state_ = ModelViewer::State::eIdle;
};
