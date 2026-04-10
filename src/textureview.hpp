#pragma once
#include "glview.hpp"

#include "graphics/mesh.hpp"
#include "graphics/shader.hpp"
#include "graphics/texture.hpp"

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

protected:
    auto OnInitializeGL() -> void override;
    auto OnRender() -> void override;

    auto OnIdle(wxIdleEvent &event) -> void;

private:
    enum class State {
        eIdle,
        eGraphicsError,
        eInvalidTexture,
        eReady,
    };

    std::span<const uint8_t> gxt_buffer_;
    std::optional<gl::ShaderProgram> shader_texture_;
    std::optional<gl::ShaderProgram> shader_background_;
    std::optional<gl::Texture> texture_;
    std::optional<gl::Mesh<TexturedVertex>> mesh_;

    State state_ = TextureViewer::State::eIdle;
};
