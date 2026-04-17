#pragma once
#include <cstdint>
#include <memory>
#include <span>

#include <glm/glm.hpp>

#include <graphics/gl.hpp>
#include <graphics/mesh.hpp>
#include <graphics/texture.hpp>

#include "render/render.hpp"

namespace render {

class FontContext final {
public:
    static constexpr uint32_t kFontBitmapWidth = 2048;
    static constexpr uint32_t kFontBitmapHeight = 2048;

    template <typename T> struct Rect {
        glm::vec<2, T> min;
        glm::vec<2, T> max;
    };

    struct Glyph {
        Rect<int32_t> glyph_rect;
        Rect<float> offset;
        float advance;
    };

    class TextObject final {
    public:
        TextObject(const TextObject &) = delete;
        auto operator=(const TextObject &) = delete;

        TextObject(TextObject &&) noexcept = default;
        auto operator=(TextObject &&) noexcept -> TextObject & = default;

        auto GetTexture() const -> const gl::Texture & { return *texture_; }
        auto GetMesh() const -> const gl::Mesh<TexturedVertex> & { return mesh_; }

    private:
        TextObject(std::shared_ptr<gl::Texture> texture, gl::Mesh<TexturedVertex> mesh)
            : texture_{texture}, mesh_{std::move(mesh)} {}

        std::shared_ptr<gl::Texture> texture_;
        gl::Mesh<TexturedVertex> mesh_;

        friend class FontContext;
    };

    explicit FontContext(std::span<const uint8_t> buffer);

    FontContext(const FontContext &) = delete;
    auto operator=(const FontContext &) = delete;

    FontContext(FontContext &&) = delete;
    auto operator=(FontContext &&) = delete;

    ~FontContext() noexcept;

    auto CreateTextObject(
        gl::GLContext::Executor executor, const glm::fvec2 &position, float size, std::u32string_view string) const
        -> TextObject;

private:
    class Impl;

    std::vector<uint8_t> ttf_buffer_;
    mutable std::unique_ptr<Impl> impl_;
};

} // namespace render