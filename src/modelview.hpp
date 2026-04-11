#pragma once
#include "glview.hpp"

#include "graphics/shader.hpp"
#include "graphics/texture.hpp"

#include "render/model.hpp"
#include "render/device.hpp"

class ModelViewer : public GLView {
public:
    ModelViewer(wxWindow *parent, const wxGLAttributes &attributes, std::span<const uint8_t> gmo_buffer);

protected:
    auto OnInitializeGL() -> void override;
    auto OnRender() -> void override;

    auto OnIdle(wxIdleEvent &event) -> void;
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
    std::span<const uint8_t> gmo_buffer_;

    std::unique_ptr<render::hal::RenderDeviceOpenGL40> device_;
    std::unique_ptr<render::Model> model_;

    State state_ = ModelViewer::State::eIdle;
};
