#pragma once
#include <glm/glm.hpp>

#include "common/common.hpp"
#include "graphics/gl.hpp"

namespace gl {

enum class TextureFormat {
    // 8 bits per pixel
    eR8 = GL_R8,
    eRG8 = GL_RG8,
    eRGB8 = GL_RGB8,
    eRGBA8 = GL_RGBA8,

    // 16 bits per pixel
    eR16F = GL_R16F,
    eRG16F = GL_RG16F,
    eRGB16F = GL_RGB16F,
    eRGBA16F = GL_RGBA16F,

    // 32 bits per pixel
    eR32F = GL_R32F,
    eRG32F = GL_RG32F,
    eRGB32F = GL_RGB32F,
    eRGBA32F = GL_RGBA32F,

    eDepthComponent16 = GL_DEPTH_COMPONENT16,
    eDepthComponent24 = GL_DEPTH_COMPONENT24,
    eDepthComponent32 = GL_DEPTH_COMPONENT32F,
    eDepthStencil = GL_DEPTH24_STENCIL8
};

enum class TextureMinFilter {
    eNearest = GL_NEAREST,
    eLinear = GL_LINEAR,
    eNearestMipmapNearest = GL_NEAREST_MIPMAP_NEAREST,
    eLinearMipmapNearest = GL_LINEAR_MIPMAP_NEAREST,
    eNearestMipmapLinear = GL_NEAREST_MIPMAP_LINEAR,
    eLinearMipmapLinear = GL_LINEAR_MIPMAP_LINEAR
};

enum class TextureWrap {
    eClampToEdge = GL_CLAMP_TO_EDGE,
    eClampToBorder = GL_CLAMP_TO_BORDER,
    eRepeat = GL_REPEAT,
    eMirroredRepeat = GL_MIRRORED_REPEAT
};

enum class TextureMagFilter {
    eNearest = GL_NEAREST,
    eLinear = GL_LINEAR,
};

class Texture final {
public:
    struct Parameters final {
        TextureFormat format;
        TextureMinFilter min_filter;
        TextureMagFilter mag_filter;
        TextureWrap wrap_s, wrap_t;

        constexpr Parameters(
            TextureFormat format, TextureMinFilter min_filter = TextureMinFilter::eLinear,
            TextureMagFilter mag_filter = TextureMagFilter::eNearest, TextureWrap wrap_s = TextureWrap::eRepeat,
            TextureWrap wrap_t = TextureWrap::eRepeat)
            : format(format), min_filter{min_filter}, mag_filter{mag_filter}, wrap_s{wrap_s}, wrap_t{wrap_t} {}
    };

    using Binding = std::pair<uint32_t, const Texture &>;

    template <TypedForwardRange<Binding> T, std::invocable F> static auto Use(const T &bindings, F &&f) {
        for (auto iter = std::ranges::begin(bindings); iter != std::ranges::end(bindings); ++iter) {
            const auto &handle = iter->second.handle_;
            GL_CHECK(glActiveTexture(GL_TEXTURE0 + iter->first));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, handle));
        }

        f();
    }

    template <std::invocable F> static auto Use(const std::initializer_list<Binding> &bindings, F &&f) {
        Use<decltype(bindings), F>(bindings, std::move(f));
    }

    static constexpr auto BitsPerPixel(const TextureFormat &format) -> uint8_t {
        switch (format) {
            // clang-format off
            case TextureFormat::eR8:         return 8;
            case TextureFormat::eRG8:        return 16;
            case TextureFormat::eRGB8:       return 24;
            case TextureFormat::eRGBA8:      return 32;
            case TextureFormat::eR16F:       return 16;
            case TextureFormat::eRG16F:      return 32;
            case TextureFormat::eRGB16F:     return 48;
            case TextureFormat::eRGBA16F:    return 64;
            case TextureFormat::eR32F:       return 32;
            case TextureFormat::eRG32F:      return 64;
            case TextureFormat::eRGB32F:     return 96;
            case TextureFormat::eRGBA32F:    return 128;
            default: return 0;
            // clang-format on
        }
    }

    Texture(GLContext::Executor executor, uint32_t width, uint32_t height, uint32_t levels, const Parameters &params);

    template <TypedContiguousRange<uint8_t> T>
    Texture(GLContext::Executor executor, uint32_t width, uint32_t height, const T &pixels, const Parameters &params)
        : executor_{executor}, handle_{factories::MakeTexture(executor)}, width_{width}, height_{height},
          parameters_{params} {
        assert(
            parameters_.format == TextureFormat::eR8 || parameters_.format == TextureFormat::eRG8 ||
            parameters_.format == TextureFormat::eRGB8 ||
            parameters_.format == TextureFormat::eRGBA8 && "non 8-bit color format unsupported for initialization");

        assert(width * height * BitsPerPixel(parameters_.format) == std::ranges::size(pixels) * 8);

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
            GL_CHECK(glTexImage2D(
                GL_TEXTURE_2D, 0, static_cast<GLint>(parameters_.format), width, height, 0, pixel_format,
                GL_UNSIGNED_BYTE, std::ranges::data(pixels)));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(parameters_.wrap_s)));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(parameters_.wrap_t)));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(parameters_.mag_filter)));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(parameters_.min_filter)));
        });
    }

    ~Texture() = default;
    Texture(const Texture &) = delete;
    auto operator=(const Texture &) = delete;

    Texture(Texture &&) = default;
    auto operator=(Texture &&) -> Texture & = default;

    auto GetWidth() const -> uint32_t { return width_; }
    auto GetHeight() const -> uint32_t { return height_; }
    auto GetParameters() const -> const Parameters & { return parameters_; }
    auto GetHandle() const -> const TextureHandle & { return handle_; }

    auto Bind(uint32_t slot) const -> void {
        executor_.RunOnContext([&](const auto &) {
            GL_CHECK(glActiveTexture(GL_TEXTURE0 + slot));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, handle_));
        });
    }

    auto Copy(const Texture &texture, const glm::ivec2 &offset = {0, 0}) -> void;
    auto Update(const glm::uvec2 &offset, const glm::uvec2 &size, const uint8_t *data) -> void;

    template <TypedContiguousRange<uint8_t> R>
    auto Update(const glm::uvec2 &offset, const glm::uvec2 &size, const R &data) -> void {
        assert(std::ranges::size(data) * 8 == BitsPerPixel(parameters_.format) * size.x * size.y);
        Update(offset, size, std::ranges::data(data));
    }

    auto GetExecutor() const -> const GLContext::Executor & { return executor_; }

private:
    GLContext::Executor executor_;
    TextureHandle handle_;
    uint32_t width_, height_;
    Parameters parameters_;
};

} // namespace gl
