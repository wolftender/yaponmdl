#pragma once
#include "graphics/gl.hpp"

#include <memory>

#include <wx/wx.h>
#include <wx/glcanvas.h>

class GLView : public wxGLCanvas {
public:
    class Context : public gl::GLContext {
    protected:
        auto StartContext() const -> void override;
        auto EndContext() const -> void override;
        auto LogMessage(std::string_view message) const -> void override;

    public:
        friend class GLView;

        Context(const Context &) = delete;
        auto operator=(const Context &) = delete;

        Context(Context &&) = delete;
        auto operator=(Context &&) = delete;

        auto IsReady() const -> bool override;
        auto GetOwnerThread() const -> std::thread::id override;
        auto IsContextThread() const -> bool override;
        auto HasExtension(std::string_view extension) const -> bool override;
        auto GetVersion() const -> std::pair<uint32_t, uint32_t> override;
        auto SetCurrent() const -> void;

    private:
        static auto LoadFunctionPointers() -> void;
        auto Finalize() -> void;

        Context(GLView *parent);

        bool is_ready_ = false;
        GLView *view_ = nullptr;

        std::thread::id owner_thread_;
        uint32_t version_major_, version_minor_;

        std::unique_ptr<wxGLContext> context_;
    };

    static auto CreateAttributes() -> wxGLAttributes;

    GLView(wxWindow *parent, const wxGLAttributes &attributes);
    virtual ~GLView() noexcept = default;

    auto GetContext() const -> const Context & { return *context_; }
    auto Render() -> void;

protected:
    virtual auto OnInitializeGL() -> void = 0;
    virtual auto OnRender() -> void = 0;

private:
    auto InitializeOpenGL() -> void;

    auto OnPaint(wxPaintEvent &event) -> void;
    auto OnSize(wxSizeEvent &event) -> void;

    std::unique_ptr<Context> context_ = nullptr;
};
