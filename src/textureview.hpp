#pragma once
#include "glview.hpp"

#include "graphics/mesh.hpp"
#include "graphics/shader.hpp"
#include "graphics/texture.hpp"

#include "render/text.hpp"

class TextureViewer : public GLView {
public:
    struct TexturedVertex {
        glm::fvec3 position;
        glm::fvec2 uv;

        TexturedVertex() : position{0.0f, 0.0f, 0.0f}, uv{0.0f, 0.0f} {}
        TexturedVertex(const glm::fvec3 &position, const glm::fvec2 &uv) : position{position}, uv{uv} {}
        TexturedVertex(float x, float y, float z, float u, float v) : position{x, y, z}, uv{u, v} {}

        static auto GetLayout() -> std::span<const gl::LayoutElement> {
            static constexpr std::array<gl::LayoutElement, 2> kLayout{
                gl::LayoutElement{
                    0, 3, gl::ShaderAttribType::eFloat, sizeof(TexturedVertex), offsetof(TexturedVertex, position)},
                gl::LayoutElement{
                    1, 2, gl::ShaderAttribType::eFloat, sizeof(TexturedVertex), offsetof(TexturedVertex, uv)}};

            return kLayout;
        }
    };

    TextureViewer(wxWindow *parent, const wxGLAttributes &attributes, std::span<const uint8_t> gxt_buffer);

    auto ZoomIn() -> void;
    auto ZoomOut() -> void;
    auto ResetView() -> void;

protected:
    auto OnInitializeGL() -> void override;
    auto OnRender() -> void override;

    auto OnIdle(wxIdleEvent &event) -> void;
    auto OnMouseScroll(wxMouseEvent &event) -> void;
    auto OnMouseMotion(wxMouseEvent &event) -> void;
    auto OnKeyDown(wxKeyEvent &event) -> void;

    auto NextTexture() -> void;
    auto PrevTexture() -> void;
    auto RefreshText() -> void;
    auto ClampZoom() -> void;

private:
    enum class State {
        eIdle,
        eGraphicsError,
        eInvalidTexture,
        eReady,
    };

    float zoom_ = 1.0f;
    glm::fvec2 center_ = glm::fvec2{0.0f};
    std::optional<glm::ivec2> prev_mouse_pos_ = std::nullopt;

    render::FontContext font_context_;
    std::optional<gl::ShaderProgram> text_shader_;
    std::optional<render::FontContext::TextObject> label_text_;

    std::span<const uint8_t> gxt_buffer_;
    std::optional<gl::ShaderProgram> shader_texture_;
    std::optional<gl::ShaderProgram> shader_background_;
    std::optional<gl::Mesh<TexturedVertex>> mesh_;

    uint32_t texture_index_ = 0;
    std::vector<gl::Texture> textures_;

    State state_ = TextureViewer::State::eIdle;
};
