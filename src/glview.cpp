#include "glview.hpp"

auto GLView::Context::StartContext() const -> void {
    if (!is_ready_) {
        throw std::runtime_error{"opengl context was not ready"};
    }

    view_->SetCurrent(*context_);
}

auto GLView::Context::EndContext() const -> void {}
auto GLView::Context::GetOwnerThread() const -> std::thread::id { return owner_thread_; }
auto GLView::Context::IsContextThread() const -> bool { return std::this_thread::get_id() == owner_thread_; }
auto GLView::Context::HasExtension([[maybe_unused]] std::string_view extension) const -> bool { return false; }
auto GLView::Context::GetVersion() const -> std::pair<uint32_t, uint32_t> { return {version_major_, version_minor_}; }
auto GLView::Context::IsReady() const -> bool { return is_ready_; }

auto GLView::Context::LoadFunctionPointers() -> void {
    static bool s_loaded_pointers = false;
    if (s_loaded_pointers) {
        return;
    }

    if (!gladLoadGL()) {
        throw std::runtime_error{"cannot initialize opengl function pointers"};
    }

    s_loaded_pointers = true;
}

auto GLView::Context::Finalize() -> void {
    view_->SetCurrent(*context_);
    LoadFunctionPointers();

    is_ready_ = true;
}

GLView::Context::Context(GLView *parent)
    : view_{parent}, owner_thread_{std::this_thread::get_id()}, version_major_{4}, version_minor_{0} {
    wxGLContextAttrs context_attribs = {};
    context_attribs.PlatformDefaults().CoreProfile().OGLVersion(4, 0).EndList();

    context_.reset(new wxGLContext(parent, nullptr, &context_attribs));
    if (!context_->IsOK()) {
        throw std::runtime_error{"cannot initialize opengl context"};
    }
}

auto GLView::CreateAttributes() -> wxGLAttributes {
    wxGLAttributes gl_attributes = {};
    gl_attributes.PlatformDefaults().Defaults().EndList();

    auto supported = wxGLCanvas::IsDisplaySupported(gl_attributes);
    if (supported) {
        return gl_attributes;
    }

    gl_attributes.Reset();
    gl_attributes.PlatformDefaults().RGBA().DoubleBuffer().Depth(16).EndList();

    supported = wxGLCanvas::IsDisplaySupported(gl_attributes);
    if (supported) {
        return gl_attributes;
    }

    throw std::runtime_error{"cannot find supported opengl configuration"};
}

GLView::GLView(wxWindow *parent, const wxGLAttributes &attributes) : wxGLCanvas{parent, attributes} {
    context_.reset(new Context(this));

    Bind(wxEVT_PAINT, &GLView::OnPaint, this);
    Bind(wxEVT_SIZE, &GLView::OnSize, this);
}

auto GLView::Render() -> void {
    if (!IsShownOnScreen()) {
        return;
    }

    if (!context_->IsReady()) {
        return;
    }

    context_->GetExecutor().RunOnContext([&]([[maybe_unused]] gl::GLContext &) { OnRender(); });
    SwapBuffers();
}

auto GLView::InitializeOpenGL() -> void {
    if (context_->IsReady()) {
        return;
    }

    if (!context_) {
        return;
    }

    context_->Finalize();
    OnInitializeGL();
}

auto GLView::OnPaint([[maybe_unused]] wxPaintEvent &event) -> void {
    [[maybe_unused]] wxPaintDC dc{this}; // dummy

    if (!IsShownOnScreen()) {
        return;
    }

    InitializeOpenGL();
    Render();
}

auto GLView::OnSize(wxSizeEvent &event) -> void {
    event.Skip();

    if (!IsShownOnScreen()) {
        return;
    }

    InitializeOpenGL();
    Refresh(false);
}
