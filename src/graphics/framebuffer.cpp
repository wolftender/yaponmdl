#include "graphics/framebuffer.hpp"

namespace gl {

Framebuffer::Framebuffer(GLContext::Executor executor, uint32_t width, uint32_t height, const Description &description)
    : executor_{executor}, width_{width}, height_{height}, handle_{factories::MakeFramebuffer(executor)},
      description_{description}, depth_{executor} {
    executor_.RunOnContext([&](const auto &) {
        GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, handle_));

        std::vector<GLenum> active_attachments;
        for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
            if (description_.color_attachments[i]) {
                const auto &attachment = *description_.color_attachments[i];

                assert(attachment.GetWidth() == width_);
                assert(attachment.GetHeight() == height_);

                GL_CHECK(glBindTexture(GL_TEXTURE_2D, attachment.GetHandle()));

                const auto attachment_id = GL_COLOR_ATTACHMENT0 + i;
                GL_CHECK(
                    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment_id, GL_TEXTURE_2D, attachment.GetHandle(), 0));

                active_attachments.push_back(attachment_id);
            }
        }

        // if depth attachment not supplied, use renderbuffer dummy
        if (description_.depth_attachment) {
            const auto &attachment = *description_.depth_attachment;

            assert(attachment.GetWidth() == width_);
            assert(attachment.GetHeight() == height_);

            GL_CHECK(glBindTexture(GL_TEXTURE_2D, attachment.GetHandle()));
            GL_CHECK(
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, attachment.GetHandle(), 0));
        } else {
            depth_ = factories::MakeRenderbuffer(executor_);

            GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, depth_));
            GL_CHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width_, height_));
            GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_));
        }

        GL_CHECK(glDrawBuffers(static_cast<GLsizei>(active_attachments.size()), active_attachments.data()));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
        GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));
    });
}

auto Framebuffer::ClearColor(const glm::fvec4 &color) -> void {
    executor_.RunOnContext([&](const auto &) {
        GL_CHECK(glClearColor(color.x, color.y, color.z, color.w));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
    });
}

auto Framebuffer::ClearColorDepth(const glm::fvec4 &color) -> void {
    executor_.RunOnContext([&](const auto &) {
        GL_CHECK(glClearColor(color.x, color.y, color.z, color.w));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    });
}

auto Framebuffer::ClearColorDepthStencil(const glm::fvec4 &color) -> void {
    executor_.RunOnContext([&](const auto &) {
        GL_CHECK(glClearColor(color.x, color.y, color.z, color.w));
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    });
}

MultisampleFramebuffer::MultisampleFramebuffer(
    GLContext::Executor executor, uint32_t width, uint32_t height, uint32_t samples,
    const Framebuffer::Description &description)
    : executor_{executor}, samples_{samples}, framebuffer_{executor, width, height, description},
      msaa_handle_{factories::MakeFramebuffer(executor)}, msaa_depth_{executor} {
    executor_.RunOnContext([&](const auto &) {
        glBindFramebuffer(GL_FRAMEBUFFER, msaa_handle_);

        std::vector<GLenum> active_attachments;

        for (uint32_t i = 0; i < Framebuffer::kMaxColorAttachments; ++i) {
            if (description.color_attachments[i]) {
                const auto &attachment = *description.color_attachments[i];
                const auto &parameters = attachment.GetParameters();

                msaa_color_textures_[i] = factories::MakeTexture(executor_);

                GL_CHECK(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, *msaa_color_textures_[i]));
                GL_CHECK(glTexImage2DMultisample(
                    GL_TEXTURE_2D_MULTISAMPLE, samples_, static_cast<GLint>(parameters.format), GetWidth(), GetHeight(),
                    GL_TRUE));
                GL_CHECK(glTexParameteri(
                    GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_S, static_cast<GLint>(parameters.wrap_s)));
                GL_CHECK(glTexParameteri(
                    GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_WRAP_T, static_cast<GLint>(parameters.wrap_t)));
                GL_CHECK(glTexParameteri(
                    GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(parameters.mag_filter)));
                GL_CHECK(glTexParameteri(
                    GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(parameters.min_filter)));

                const auto attachment_id = GL_COLOR_ATTACHMENT0 + i;
                GL_CHECK(glFramebufferTexture2D(
                    GL_FRAMEBUFFER, attachment_id, GL_TEXTURE_2D_MULTISAMPLE, *msaa_color_textures_[i], 0));

                active_attachments.push_back(attachment_id);
            }
        }

        // if depth attachment not supplied, use renderbuffer dummy
        if (description.depth_attachment) {
            const auto &attachment = *description.depth_attachment;
            const auto &parameters = attachment.GetParameters();

            msaa_depth_texture_ = factories::MakeTexture(executor_);

            GL_CHECK(glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, *msaa_depth_texture_));
            GL_CHECK(glTexImage2DMultisample(
                GL_TEXTURE_2D_MULTISAMPLE, samples_, static_cast<GLint>(parameters.format), GetWidth(), GetHeight(),
                GL_TRUE));
            GL_CHECK(
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, *msaa_depth_texture_, 0));
        } else {
            msaa_depth_ = factories::MakeRenderbuffer(executor_);

            GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, msaa_depth_));
            GL_CHECK(glRenderbufferStorageMultisample(
                GL_RENDERBUFFER, samples_, GL_DEPTH24_STENCIL8, GetWidth(), GetHeight()));
            GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaa_depth_));
        }

        GL_CHECK(glDrawBuffers(static_cast<GLsizei>(active_attachments.size()), active_attachments.data()));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
        GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));
    });
}

auto MultisampleFramebuffer::ClearColor(const glm::fvec4 &color) -> void { framebuffer_.ClearColor(color); }

auto MultisampleFramebuffer::ClearColorDepth(const glm::fvec4 &color) -> void { framebuffer_.ClearColorDepth(color); }

auto MultisampleFramebuffer::ClearColorDepthStencil(const glm::fvec4 &color) -> void {
    framebuffer_.ClearColorDepthStencil(color);
}

} // namespace gl
