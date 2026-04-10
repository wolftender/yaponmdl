#pragma once
#include "glview.hpp"

class TextureViewer : public GLView {
public:
    TextureViewer(wxWindow *parent, const wxGLAttributes &attributes) : GLView{parent, attributes} {
        Bind(wxEVT_IDLE, &TextureViewer::OnIdle, this);
    }

protected:
    auto OnInitializeGL() -> void override;
    auto OnRender() -> void override;

    auto OnIdle(wxIdleEvent &event) -> void;
};
