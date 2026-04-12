#pragma once
#include "glview.hpp"

#include "render/camera.hpp"
#include "render/model.hpp"
#include "render/device.hpp"

class ModelViewer : public GLView {
public:
    ModelViewer(wxWindow *parent, const wxGLAttributes &attributes, std::span<const uint8_t> gmo_buffer);

protected:
    auto OnInitializeGL() -> void override;
    auto OnRender() -> void override;

    auto OnIdle(wxIdleEvent &event) -> void;
    auto OnSize(wxSizeEvent &event) -> void;
    auto OnMouseScroll(wxMouseEvent &event) -> void;

private:
    enum class State {
        eIdle,
        eGraphicsError,
        eInvalidModel,
        eInvalidTexture,
        eReady,
    };

    float zoom_ = 1.0f;

    std::optional<glm::uvec2> pending_resize_;
    std::span<const uint8_t> gmo_buffer_;

    render::PerspectiveCamera camera_;
    std::unique_ptr<render::hal::RenderDeviceOpenGL40> device_;
    std::unique_ptr<render::Model> model_;
    std::optional<render::Model::Controller> controller_;

    State state_ = ModelViewer::State::eIdle;
};
