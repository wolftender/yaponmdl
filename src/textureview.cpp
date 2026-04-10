#include "textureview.hpp"

auto TextureViewer::OnInitializeGL() -> void {
    GL_IMPLEMENTATION_INTERNAL;
    GL_CHECK(glEnable(GL_BLEND));
}

auto TextureViewer::OnRender() -> void {
    GL_IMPLEMENTATION_INTERNAL;

    const auto current_size = GetSize();
    GL_CHECK(glViewport(0, 0, current_size.x, current_size.y));
    GL_CHECK(glClearColor(1.0f, 0.5f, 0.0f, 1.0f));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
}

auto TextureViewer::OnIdle([[maybe_unused]] wxIdleEvent &event) -> void { Render(); }
