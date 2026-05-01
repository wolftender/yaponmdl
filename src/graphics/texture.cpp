#include "graphics/texture.hpp"

namespace gl {

static auto FormatToFormat(gl::TextureFormat format) -> GLenum {
    switch (format) {
    case TextureFormat::eR8:
    case TextureFormat::eR16F:
    case TextureFormat::eR32F:
        return GL_RED;

    case TextureFormat::eRG8:
    case TextureFormat::eRG16F:
    case TextureFormat::eRG32F:
        return GL_RG;

    case TextureFormat::eRGB8:
    case TextureFormat::eRGB16F:
    case TextureFormat::eRGB32F:
        return GL_RGB;

    case TextureFormat::eRGBA8:
    case TextureFormat::eRGBA16F:
    case TextureFormat::eRGBA32F:
        return GL_RGBA;

    case TextureFormat::eDepthComponent16:
    case TextureFormat::eDepthComponent24:
    case TextureFormat::eDepthComponent32:
        return GL_DEPTH_COMPONENT;

    case TextureFormat::eDepthStencil:
        return GL_DEPTH_STENCIL;

    default:
        throw std::runtime_error{"invalid gl format"};
    }
}

static auto FormatToType(gl::TextureFormat format) -> GLenum {
    switch (format) {
    case TextureFormat::eR8:
    case TextureFormat::eRG8:
    case TextureFormat::eRGB8:
    case TextureFormat::eRGBA8:
        return GL_UNSIGNED_BYTE;

    case TextureFormat::eR16F:
    case TextureFormat::eRG16F:
    case TextureFormat::eRGB16F:
    case TextureFormat::eRGBA16F:
        return GL_HALF_FLOAT;

    case TextureFormat::eR32F:
    case TextureFormat::eRG32F:
    case TextureFormat::eRGB32F:
    case TextureFormat::eRGBA32F:
        return GL_FLOAT;

    case TextureFormat::eDepthComponent16:
        return GL_UNSIGNED_SHORT;

    case TextureFormat::eDepthComponent24:
    case TextureFormat::eDepthComponent32:
        return GL_UNSIGNED_INT;

    case TextureFormat::eDepthStencil:
        return GL_UNSIGNED_INT_24_8;

    default:
        throw std::runtime_error{"invalid gl type"};
    }
}

Texture::Texture(
    GLContext::Executor executor, uint32_t width, uint32_t height, uint32_t levels, const Parameters &params)
    : executor_{executor}, handle_{factories::MakeTexture(executor)}, width_{width}, height_{height},
      parameters_{params} {
    executor_.RunOnContext([&](const auto &) {
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, handle_));

        uint32_t level_width = width;
        uint32_t level_height = height;
        for (uint32_t level = 0; level < levels; ++level) {
            GL_CHECK(glTexImage2D(
                GL_TEXTURE_2D, level, static_cast<GLint>(parameters_.format), level_width, level_height, 0,
                FormatToFormat(parameters_.format), FormatToType(parameters_.format), nullptr));

            level_width = level_width / 2;
            level_height = level_height / 2;

            if (level_width == 0) {
                level_width = 0;
            }

            if (level_height == 0) {
                level_height = 0;
            }
        }

        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(parameters_.wrap_s)));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(parameters_.wrap_t)));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(parameters_.mag_filter)));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(parameters_.min_filter)));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
    });
}

auto Texture::Copy(const Texture &texture, const glm::ivec2 &offset) -> void {
    executor_.RunOnContext([&](const auto &) {
        FramebufferHandle fb_source{factories::MakeFramebuffer(executor_)};
        FramebufferHandle fb_dest{factories::MakeFramebuffer(executor_)};

        GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, fb_source));
        GL_CHECK(glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.handle_, 0));
        GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb_dest));
        GL_CHECK(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, handle_, 0));

        const auto status_source = glCheckFramebufferStatus(fb_source);
        const auto status_dest = glCheckFramebufferStatus(fb_dest);

        if (GL_FRAMEBUFFER_COMPLETE != status_source || GL_FRAMEBUFFER_COMPLETE != status_dest) {
            throw std::runtime_error{"cannot copy textures using framebuffer"};
        }

        int32_t src_x0 = offset.x >= 0 ? 0 : -offset.x;
        int32_t src_y0 = offset.y >= 0 ? 0 : -offset.y;
        int32_t src_x1 = offset.x + texture.width_ > width_ ? width_ - offset.x : texture.width_;
        int32_t src_y1 = offset.y + texture.height_ > height_ ? height_ - offset.y : texture.height_;
        int32_t dst_x0 = offset.x;
        int32_t dst_y0 = offset.y;
        int32_t dst_x1 = std::min(offset.x + texture.width_, width_);
        int32_t dst_y1 = std::min(offset.y + texture.height_, height_);

        GL_CHECK(glBlitFramebuffer(
            src_x0, src_y0, src_x1, src_y1, dst_x0, dst_y0, dst_x1, dst_y1, GL_COLOR_BUFFER_BIT, GL_NEAREST));
        GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, 0));
        GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
    });
}

auto Texture::Update(const glm::uvec2 &offset, const glm::uvec2 &size, const uint8_t *data) -> void {
    assert(offset.x + size.x <= width_);
    assert(offset.y + size.y <= height_);

    GLint pixel_format = 0;
    switch (parameters_.format) {
    case TextureFormat::eR8:
        pixel_format = GL_RED;
        break;
    case TextureFormat::eRG8:
        pixel_format = GL_RG;
        break;
    case TextureFormat::eRGB8:
        pixel_format = GL_RGB;
        break;
    case TextureFormat::eRGBA8:
        pixel_format = GL_RGBA;
        break;
    default:
        throw std::runtime_error("non 8-bit color format unsupported for initialization");
    }

    executor_.RunOnContext([&](const auto &) {
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, handle_));
        GL_CHECK(glTexSubImage2D(
            GL_TEXTURE_2D, 0, offset.x, offset.y, size.x, size.y, pixel_format, GL_UNSIGNED_BYTE, data));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
    });
}

} // namespace gl
