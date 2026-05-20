#include <array>

#include "textureview.hpp"
#include "resourcestore.hpp"

#include "formats/gxt.hpp"

class GxtWxLogger : public gxt::GxtLogger {
public:
    auto log(std::string_view message) const -> void override {
        wxLogMessage("libgxt message: " + wxString{message.data(), message.size()});
    }
};

TextureViewer::TextureViewer(wxWindow *parent, const wxGLAttributes &attributes, std::span<const uint8_t> gxt_buffer)
    : GLView{parent, attributes}, font_context_{memoryresource::GetFontOpensansTtf()}, gxt_buffer_{gxt_buffer} {
    Bind(wxEVT_IDLE, &TextureViewer::OnIdle, this);
    Bind(wxEVT_MOUSEWHEEL, &TextureViewer::OnMouseScroll, this);
    Bind(wxEVT_MOTION, &TextureViewer::OnMouseMotion, this);
    Bind(wxEVT_KEY_DOWN, &TextureViewer::OnKeyDown, this);
}

auto TextureViewer::ZoomIn() -> void {
    zoom_ = zoom_ + 0.25f;
    ClampZoom();
}

auto TextureViewer::ZoomOut() -> void {
    zoom_ = zoom_ - 0.25f;
    ClampZoom();
}

auto TextureViewer::ResetView() -> void {
    zoom_ = 1.0f;
    center_ = {0.0f, 0.0f};
}

auto TextureViewer::OnInitializeGL() -> void {
    GL_IMPLEMENTATION_INTERNAL;

    try {
        text_shader_.emplace(
            gl::ShaderProgram{
                GetContext().GetExecutor(), memoryresource::GetVertShaderText(), memoryresource::GetFragShaderText()});
    } catch (const std::exception &e) {
        wxLogError(wxString::Format("model viewer fatal error, cannot initialize text rendering: %s", e.what()));
        state_ = State::eGraphicsError;
        return;
    }

    try {
        shader_texture_.emplace(
            gl::ShaderProgram{
                GetContext().GetExecutor(), memoryresource::GetVertShaderTexture(),
                memoryresource::GetFragShaderTexture()});

        shader_background_.emplace(
            gl::ShaderProgram{
                GetContext().GetExecutor(), memoryresource::GetVertShaderTexture(),
                memoryresource::GetFragShaderChecker()});
    } catch (const std::exception &e) {
        wxLogError(wxString::Format("cannot compile shader program: %s", e.what()));
        state_ = State::eGraphicsError;
        return;
    }

    wxLogMessage("compiled shader program successfully");

    const std::array<TexturedVertex, 6> quad_verts = {
        TexturedVertex{-1.0f, +1.0f, +0.0f, 0.0f, 1.0f}, TexturedVertex{+1.0f, +1.0f, +0.0f, 1.0f, 1.0f},
        TexturedVertex{+1.0f, -1.0f, +0.0f, 1.0f, 0.0f}, TexturedVertex{-1.0f, +1.0f, +0.0f, 0.0f, 1.0f},
        TexturedVertex{+1.0f, -1.0f, +0.0f, 1.0f, 0.0f}, TexturedVertex{-1.0f, -1.0f, +0.0f, 0.0f, 0.0f},
    };

    try {
        mesh_.emplace(gl::Mesh<TexturedVertex>{GetContext().GetExecutor(), quad_verts});
    } catch (const std::exception &e) {
        wxLogError(wxString::Format("cannot create mesh for texture: %s", e.what()));
        state_ = State::eGraphicsError;
        return;
    }

    // attempt to load the gxt texture
    std::vector<gxt::GxtImageBitmap> tex_bitmaps;
    GxtWxLogger gxt_logger;

    try {
        tex_bitmaps = gxt::LoadBitmaps(gxt_buffer_, &gxt_logger);
    } catch (const std::exception &e) {
        wxLogError(wxString::Format("libgxt fatal error: %s", e.what()));
        state_ = State::eInvalidTexture;
        return;
    }

    // we only expect one bitmap here from libgxt
    for (const auto &bitmap : tex_bitmaps) {
        try {
            const gl::Texture::Parameters parameters = {gl::TextureFormat::eRGBA8};
            textures_.emplace_back(
                gl::Texture{GetContext().GetExecutor(), bitmap.width, bitmap.height, bitmap.rgba_plane, parameters});
        } catch (const std::exception &e) {
            wxLogError(wxString::Format("cannot create texture: %s", e.what()));
        }
    }

    wxLogMessage(
        wxString::Format(
            "loaded %d bitmaps from the gxt file, successfully created %d textures",
            static_cast<uint32_t>(tex_bitmaps.size()), static_cast<uint32_t>(textures_.size())));

    if (textures_.empty()) {
        wxLogError("failed to load any texture from the file");

        state_ = State::eGraphicsError;
        return;
    }

    RefreshText();
    state_ = State::eReady;
}

auto TextureViewer::OnRender() -> void {
    GL_IMPLEMENTATION_INTERNAL;

    if (state_ != State::eReady || textures_.empty()) {
        return;
    }

    const auto &texture = textures_[texture_index_ % textures_.size()];

    const auto current_size = GetSize();
    GL_CHECK(glViewport(0, 0, current_size.x, current_size.y));

    // enable features needed for blending
    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glDisable(GL_DEPTH_TEST));
    GL_CHECK(glEnable(GL_MULTISAMPLE));
    GL_CHECK(glDisable(GL_CULL_FACE));
    GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    GL_CHECK(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

    const glm::fmat4x4 identity = glm::fmat4x4{1.0f};

    const auto fwidth = static_cast<float>(current_size.x);
    const auto fheight = static_cast<float>(current_size.y);
    const auto aspect = fwidth / fheight;
    const auto tex_aspect = static_cast<float>(texture.GetWidth()) / static_cast<float>(texture.GetHeight());

    shader_background_->Use([&](const gl::ShaderProgram::Context &program_context) {
        program_context.SetUniform("u_view_projection", identity);
        program_context.SetUniform("u_divisions", static_cast<float>(current_size.y) / 30.0f);
        program_context.SetUniform("u_color1", glm::fvec3{0.33f, 0.33f, 0.33f});
        program_context.SetUniform("u_color2", glm::fvec3{0.66f, 0.66f, 0.66f});
        program_context.SetUniform("u_aspect", aspect);
        mesh_->Draw();
    });

    // clang-format off
    const glm::fmat4x4 aspect_fix = glm::fmat4x4{
        1.0f / aspect * zoom_, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f * zoom_ / tex_aspect, 0.0f, 0.0f,
        0.0f , 0.0f, 1.0f, 0.0f,
        2.0f * center_.x / fwidth, 2.0f * center_.y /fheight, 0.0f, 1.0f,
    };
    // clang-format on

    shader_texture_->Use([&](const gl::ShaderProgram::Context &program_context) {
        program_context.SetUniform("u_view_projection", aspect_fix);
        program_context.SetSampler("u_texture", 0);

        gl::Texture::Use({gl::Texture::Binding{0, texture}}, [&]() { mesh_->Draw(); });
    });

    // clang-format off
    const glm::fmat4x4 text_matrix = {
        2.0f / fwidth, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / fheight, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f,
    };
    // clang-format on

    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    if (!label_text_.has_value()) {
        return;
    }

    text_shader_->Use([&](const gl::ShaderProgram::Context &context) {
        gl::Texture::Use({gl::Texture::Binding{0, label_text_->GetTexture()}}, [&]() {
            context.SetUniform("u_view_projection", text_matrix);
            context.SetUniform("u_texture", 0);
            label_text_->GetMesh().Draw();
        });
    });
}

auto TextureViewer::OnIdle([[maybe_unused]] wxIdleEvent &event) -> void { Render(); }

auto TextureViewer::OnMouseScroll(wxMouseEvent &event) -> void {
    auto delta = event.GetWheelRotation() / event.GetWheelDelta();
    zoom_ = zoom_ + (delta * 0.05f);

    ClampZoom();
}

auto TextureViewer::OnMouseMotion([[maybe_unused]] wxMouseEvent &event) -> void {
    glm::ivec2 current_mouse_position;
    event.GetPosition(&current_mouse_position.x, &current_mouse_position.y);

    if (event.Dragging() && event.LeftIsDown()) {
        if (!prev_mouse_pos_.has_value()) {
            prev_mouse_pos_ = current_mouse_position;
            return;
        }

        const auto delta_position = current_mouse_position - prev_mouse_pos_.value();
        prev_mouse_pos_ = current_mouse_position;
        center_ = center_ - glm::fvec2{-static_cast<float>(delta_position.x), static_cast<float>(delta_position.y)};

    } else {
        prev_mouse_pos_ = std::nullopt;
    }
}

auto TextureViewer::OnKeyDown(wxKeyEvent &event) -> void {
    const auto uc = event.GetUnicodeKey();
    if (textures_.empty()) {
        return;
    }

    switch (uc) {
    case 'A':
    case '-':
        PrevTexture();
        break;

    case 'D':
    case '+':
    case '=':
        NextTexture();
        break;

    default:
        break;
    }
}

auto TextureViewer::RefreshText() -> void {
    const auto content = fmt::format("Texture {} out of {}", texture_index_ + 1, textures_.size());

    std::u32string codepoints;
    const wxString wxstr{content};

    codepoints.reserve(wxstr.Length());
    for (const wxUniChar unicode_char : wxstr) {
        codepoints.push_back(unicode_char.GetValue());
    }

    try {
        label_text_.emplace(
            font_context_.CreateTextObject(GetContext().GetExecutor(), {10.0f, 10.0f}, 32.0f, codepoints));
    } catch (const std::exception &e) {
        wxLogError(wxString::Format("cannot update text: %s", e.what()));
    }
}

auto TextureViewer::NextTexture() -> void {
    const auto num_textures = static_cast<uint32_t>(textures_.size());
    const auto current_index = texture_index_;

    texture_index_ = (texture_index_ + 1) % num_textures;

    if (texture_index_ != current_index) {
        ResetView();
        RefreshText();
    }
}

auto TextureViewer::PrevTexture() -> void {
    const auto num_textures = static_cast<uint32_t>(textures_.size());
    const auto current_index = texture_index_;

    texture_index_ = (texture_index_ + num_textures - 1) % num_textures;

    if (texture_index_ != current_index) {
        ResetView();
        RefreshText();
    }
}

auto TextureViewer::ClampZoom() -> void { zoom_ = glm::clamp(zoom_, 0.2f, 10.0f); }
