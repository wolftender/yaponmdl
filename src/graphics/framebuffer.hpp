#pragma once
#include <optional>
#include <glm/glm.hpp>

#include "graphics/gl.hpp"
#include "graphics/texture.hpp"

namespace gl {

class Framebuffer final {
public:
    constexpr static std::size_t kMaxColorAttachments = 5;

    struct Description {
        std::array<std::shared_ptr<Texture>, kMaxColorAttachments> color_attachments;
        std::shared_ptr<Texture> depth_attachment;
    };

    Framebuffer(GLContext::Executor executor, uint32_t width, uint32_t height, const Description &description);
    Framebuffer(const Framebuffer &) = delete;
    auto operator=(const Framebuffer &) -> Framebuffer & = delete;

    Framebuffer(Framebuffer &&) = default;
    auto operator=(Framebuffer &&) -> Framebuffer & = default;

    template <std::invocable F> auto Use(F &&f) {
        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, handle_));
        f();
        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }

    auto ClearColor(const glm::fvec4 &color) -> void;
    auto ClearColorDepth(const glm::fvec4 &color) -> void;
    auto ClearColorDepthStencil(const glm::fvec4 &color) -> void;

    auto GetDescription() const -> const Description & { return description_; }
    auto GetWidth() const -> uint32_t { return width_; }
    auto GetHeight() const -> uint32_t { return height_; }
    auto GetHandle() const -> const FramebufferHandle & { return handle_; }
    auto Executor() const -> const GLContext::Executor & { return executor_; }

private:
    GLContext::Executor executor_;
    uint32_t width_, height_;
    FramebufferHandle handle_;
    Description description_;
    RenderbufferHandle depth_;
};

class MultisampleFramebuffer final {
public:
    MultisampleFramebuffer(
        GLContext::Executor executor, uint32_t width, uint32_t height, uint32_t samples,
        const Framebuffer::Description &description);
    MultisampleFramebuffer(const MultisampleFramebuffer &) = delete;
    auto operator=(const MultisampleFramebuffer &) = delete;

    MultisampleFramebuffer(MultisampleFramebuffer &&) = default;
    auto operator=(MultisampleFramebuffer &&) -> MultisampleFramebuffer & = default;

    template <std::invocable F> auto Use(F &&f) {
        executor_.RunOnContext([&](const auto &) {
            GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, msaa_handle_));
            f();
            GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

            // resolve msaa
            GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, msaa_handle_));
            GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_.GetHandle()));

            for (int att_id = 0; att_id < static_cast<int>(Framebuffer::kMaxColorAttachments); ++att_id) {
                if (msaa_color_textures_[att_id]) {
                    GL_CHECK(glReadBuffer(GL_COLOR_ATTACHMENT0 + att_id));
                    GL_CHECK(glDrawBuffer(GL_COLOR_ATTACHMENT0 + att_id));
                    GL_CHECK(glBlitFramebuffer(
                        0, 0, GetWidth(), GetHeight(), 0, 0, GetWidth(), GetHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST));
                }
            }

            GL_CHECK(glBlitFramebuffer(
                0, 0, GetWidth(), GetHeight(), 0, 0, GetWidth(), GetHeight(), GL_DEPTH_BUFFER_BIT, GL_NEAREST));

            GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, 0));
            GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
        });
    }

    auto ClearColor(const glm::fvec4 &color) -> void;
    auto ClearColorDepth(const glm::fvec4 &color) -> void;
    auto ClearColorDepthStencil(const glm::fvec4 &color) -> void;

    auto GetDescription() const -> const Framebuffer::Description & { return framebuffer_.GetDescription(); }
    auto GetWidth() const -> uint32_t { return framebuffer_.GetWidth(); }
    auto GetHeight() const -> uint32_t { return framebuffer_.GetHeight(); }
    auto GetHandle() const -> const FramebufferHandle & { return framebuffer_.GetHandle(); }
    auto GetMultisampledHandle() const -> const FramebufferHandle & { return msaa_handle_; }
    auto GetFramebuffer() const -> const Framebuffer & { return framebuffer_; }
    auto GetSamples() const -> uint32_t { return samples_; }
    auto GetExecutor() const -> const GLContext::Executor & { return executor_; }

private:
    GLContext::Executor executor_;

    uint32_t samples_;
    Framebuffer framebuffer_;
    FramebufferHandle msaa_handle_;
    RenderbufferHandle msaa_depth_;

    std::array<std::optional<TextureHandle>, Framebuffer::kMaxColorAttachments> msaa_color_textures_;
    std::optional<TextureHandle> msaa_depth_texture_;
};

} // namespace gl