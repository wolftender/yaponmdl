#include <stb_rect_pack/stb_rect_pack.h>
#include <stb_truetype/stb_truetype.h>

#include "render/text.hpp"

namespace render {

class FontContext::Impl final {
    class FontCache {
    public:
        static constexpr uint32_t kPadding = 2;

        FontCache(gl::GLContext::Executor executor, const Impl *impl, float font_size)
            : impl_{impl}, font_size_{font_size} {
            font_scale_ = stbtt_ScaleForPixelHeight(&impl->stb_font_info_, font_size);

            cpu_bitmap_.resize(kFontBitmapWidth * kFontBitmapHeight);
            std::fill(cpu_bitmap_.begin(), cpu_bitmap_.end(), 0);

            const gl::Texture::Parameters texture_desc = {
                gl::TextureFormat::eR8,   gl::TextureMinFilter::eLinear, gl::TextureMagFilter::eLinear,
                gl::TextureWrap::eRepeat, gl::TextureWrap::eRepeat,
            };

            atlas_ = std::make_shared<gl::Texture>(executor, kFontBitmapWidth, kFontBitmapHeight, 1, texture_desc);

            // initialize packing context
            auto stb_res = stbtt_PackBegin(
                &stb_pack_context_, cpu_bitmap_.data(), kFontBitmapWidth, kFontBitmapHeight, kFontBitmapWidth, kPadding,
                nullptr);
            if (!stb_res) {
                throw std::runtime_error{"failed to allocate font packing context"};
            }

            stbtt_PackSetSkipMissingCodepoints(&stb_pack_context_, 1);
            AddCodepointGlyphRange(U'!', U'~');
        }

        ~FontCache() noexcept { stbtt_PackEnd(&stb_pack_context_); }

        FontCache(const FontCache &) = delete;
        auto operator=(const FontCache &) = delete;

        // because of pack context moving is forbidden
        FontCache(FontCache &&) = delete;
        auto operator=(FontCache &&) = delete;

        auto CreateTextObject(gl::GLContext::Executor executor, const glm::fvec2 &position, std::u32string_view string)
            -> TextObject {
            const auto f_tex_width = static_cast<float>(kFontBitmapWidth);
            const auto f_tex_height = static_cast<float>(kFontBitmapHeight);

            std::vector<TexturedVertex> vertices;
            std::vector<uint32_t> indices;

            // reserve space for quads
            const auto length = string.size();
            vertices.reserve(length * 4);
            indices.reserve(length * 6);

            const auto f_ascent = static_cast<float>(impl_->font_ascent_) * font_scale_;
            const auto f_descent = static_cast<float>(impl_->font_descent_) * font_scale_;
            const auto f_line_gap = static_cast<float>(impl_->font_line_gap_) * font_scale_;

            float x_advance = 0.0f;
            float y_advance = f_ascent;

            const auto fn_insert_quad = [&](const Glyph &glyph) {
                const auto base_idx = static_cast<uint32_t>(vertices.size());

                const auto f_u0 = static_cast<float>(glyph.glyph_rect.min.x) / f_tex_width;
                const auto f_v0 = static_cast<float>(glyph.glyph_rect.min.y) / f_tex_height;
                const auto f_u1 = static_cast<float>(glyph.glyph_rect.max.x) / f_tex_width;
                const auto f_v1 = static_cast<float>(glyph.glyph_rect.max.y) / f_tex_height;

                const auto f_x0 = position.x + glyph.offset.min.x + x_advance;
                const auto f_y0 = position.y + glyph.offset.min.y + y_advance;
                const auto f_x1 = position.x + glyph.offset.max.x + x_advance;
                const auto f_y1 = position.y + glyph.offset.max.y + y_advance;

                vertices.push_back(TexturedVertex{{f_x0, f_y0, 0.0f}, {f_u0, f_v0}, {1.0f, 1.0f, 1.0f}});
                vertices.push_back(TexturedVertex{{f_x1, f_y0, 0.0f}, {f_u1, f_v0}, {1.0f, 1.0f, 1.0f}});
                vertices.push_back(TexturedVertex{{f_x1, f_y1, 0.0f}, {f_u1, f_v1}, {1.0f, 1.0f, 1.0f}});
                vertices.push_back(TexturedVertex{{f_x0, f_y1, 0.0f}, {f_u0, f_v1}, {1.0f, 1.0f, 1.0f}});

                indices.insert(
                    indices.end(),
                    {base_idx + 0, base_idx + 1, base_idx + 2, base_idx + 0, base_idx + 2, base_idx + 3});

                x_advance = x_advance + glyph.advance;
            };

            for (const auto cp : string) {
                if (cp == U'\n') {
                    y_advance = y_advance - f_descent + f_line_gap + f_ascent;
                    x_advance = 0.0f;
                    continue;
                }

                fn_insert_quad(GetGlyph(cp));
            }

            return TextObject{atlas_, gl::Mesh<TexturedVertex>{executor, vertices, indices}};
        }

    private:
        auto GetGlyph(uint32_t codepoint) -> Glyph & {
            const auto iter = glyphs_.find(codepoint);
            if (iter != glyphs_.end()) {
                return iter->second;
            }

            return AddCodepointGlyph(codepoint);
        }

        auto UploadTexture() -> void {
            atlas_->Update({0, 0}, {kFontBitmapWidth, kFontBitmapHeight}, cpu_bitmap_.data());
        }

        auto StoreGlyphData(uint32_t codepoint, const stbtt_packedchar &packed_char) -> Glyph & {
            const auto [glyph, inserted] = glyphs_.try_emplace(
                codepoint, Glyph{
                               .glyph_rect = {{packed_char.x0, packed_char.y0}, {packed_char.x1, packed_char.y1}},
                               .offset = {{packed_char.xoff, packed_char.yoff}, {packed_char.xoff2, packed_char.yoff2}},
                               .advance = packed_char.xadvance,
                           });

            return glyph->second;
        }

        auto StoreGlyphData(uint32_t first_cp, uint32_t last_cp, std::span<const stbtt_packedchar> packed_chars)
            -> void {
            assert(last_cp > first_cp && "incorrect arguments");
            for (uint32_t codepoint = first_cp, i = 0; codepoint < last_cp; ++codepoint, ++i) {
                (void)StoreGlyphData(codepoint, packed_chars[i]);
            }
        }

        auto AddCodepointGlyph(uint32_t codepoint) -> Glyph & {
            stbtt_packedchar new_glyph;
            int stb_res = stbtt_PackFontRange(
                &stb_pack_context_, impl_->ttf_buffer_.data(), 0, font_size_, codepoint, 1, &new_glyph);
            if (!stb_res) {
                throw std::runtime_error{fmt::format("failed to pack new unicode glyph {}", codepoint)};
            }

            auto &res = StoreGlyphData(codepoint, new_glyph);
            UploadTexture();

            return res;
        }

        auto AddCodepointGlyphRange(uint32_t first_cp, uint32_t last_cp) -> void {
            assert(last_cp > first_cp && "invalid codepoint range");

            uint32_t num_new_glyphs = last_cp - first_cp;
            std::vector<stbtt_packedchar> new_glyphs;
            new_glyphs.resize(num_new_glyphs);

            const auto stb_res = stbtt_PackFontRange(
                &stb_pack_context_, impl_->ttf_buffer_.data(), 0, font_size_, first_cp, last_cp - first_cp,
                new_glyphs.data());
            if (!stb_res) {
                throw std::runtime_error{
                    fmt::format("failed to pack new unicode glyphs in range {} - {}", first_cp, last_cp)};
            }

            StoreGlyphData(first_cp, last_cp, new_glyphs);
            UploadTexture();
        }

        const Impl *impl_;

        float font_size_;
        float font_scale_;

        stbtt_pack_context stb_pack_context_;
        std::vector<uint8_t> cpu_bitmap_;
        std::shared_ptr<gl::Texture> atlas_;

        std::unordered_map<uint32_t, Glyph> glyphs_;
    };

public:
    Impl(std::span<const uint8_t> ttf_buffer) : ttf_buffer_{ttf_buffer} {
        auto stb_res = stbtt_InitFont(&stb_font_info_, ttf_buffer_.data(), 0);
        if (!stb_res) {
            throw std::runtime_error{"failed to initialize stb ttf implementation"};
        }

        stbtt_GetFontVMetrics(&stb_font_info_, &font_ascent_, &font_descent_, &font_line_gap_);
    }

    ~Impl() noexcept = default;

    Impl(const Impl &) = delete;
    auto operator=(const Impl &) = delete;

    Impl(Impl &&) noexcept = delete;
    auto operator=(Impl &&) noexcept = delete;

    auto CreateTextObject(
        gl::GLContext::Executor executor, const glm::fvec2 &position, float size, std::u32string_view string)
        -> TextObject {
        auto iter = cache_.find(size);
        if (iter != cache_.end()) {
            return iter->second->CreateTextObject(executor, position, string);
        }

        auto [new_cache, inserted] = cache_.emplace(size, std::make_unique<FontCache>(executor, this, size));
        return new_cache->second->CreateTextObject(executor, position, string);
    }

private:
    stbtt_fontinfo stb_font_info_;

    int32_t font_ascent_;
    int32_t font_descent_;
    int32_t font_line_gap_;

    std::span<const uint8_t> ttf_buffer_;
    std::unordered_map<float, std::unique_ptr<FontCache>> cache_;
};

FontContext::FontContext(std::span<const uint8_t> buffer) : ttf_buffer_{buffer.begin(), buffer.end()} {
    impl_ = std::make_unique<Impl>(ttf_buffer_);
}

FontContext::~FontContext() noexcept = default;

auto FontContext::CreateTextObject(
    gl::GLContext::Executor executor, const glm::fvec2 &position, float size, std::u32string_view string) const
    -> TextObject {
    return impl_->CreateTextObject(executor, position, size, string);
}

} // namespace render
