#include <fstream>
#include <fmt/xchar.h>

#include "formats/gmo.hpp"
#include "formats/act.hpp"

#include "modelview.hpp"
#include "modelconv.hpp"

#include "generated/frag_mesh.glsl.h"
#include "generated/vert_skinmesh.glsl.h"
#include "generated/vert_staticmesh.glsl.h"
#include "generated/vert_postfilter.glsl.h"
#include "generated/frag_postfilter.glsl.h"
#include "generated/vert_text.glsl.h"
#include "generated/frag_text.glsl.h"
#include "generated/opensans.ttf.h"

class GmoWxLogger : public gmo::GmoLogger {
public:
    auto log(std::string_view message) const -> void override {
        wxLogMessage("libgmo message: %s ", wxString{message.data(), message.size()});
    }
};

ModelViewer::ModelViewer(wxWindow *parent, const wxGLAttributes &attributes, std::span<const uint8_t> gmo_buffer)
    : GLView{parent, attributes}, font_context_{kOpensans_ttf}, gmo_buffer_{gmo_buffer} {

    camera_.SetNear(0.1f);
    camera_.SetFar(300.0f);

    animation_timer_ = std::make_unique<AnimationTimer>(this);

    Bind(wxEVT_SIZE, &ModelViewer::OnSize, this);
    Bind(wxEVT_IDLE, &ModelViewer::OnIdle, this);
    Bind(wxEVT_MOUSEWHEEL, &ModelViewer::OnMouseScroll, this);
    Bind(wxEVT_MOTION, &ModelViewer::OnMouseMotion, this);
}

auto ModelViewer::OnInitializeGL() -> void {
    GL_IMPLEMENTATION_INTERNAL;

    render::hal::RenderDeviceOpenGL40::Description hal_desc = {
        .fs_static_source =
            std::string_view{reinterpret_cast<const char *>(kFragMesh_glsl.data()), kFragMesh_glsl.size()},
        .vs_static_source =
            std::string_view{reinterpret_cast<const char *>(kVertStaticmesh_glsl.data()), kVertStaticmesh_glsl.size()},
        .fs_skinned_source =
            std::string_view{reinterpret_cast<const char *>(kFragMesh_glsl.data()), kFragMesh_glsl.size()},
        .vs_skinned_source =
            std::string_view{reinterpret_cast<const char *>(kVertSkinmesh_glsl.data()), kVertSkinmesh_glsl.size()},
        .fs_post_filter =
            std::string_view{reinterpret_cast<const char *>(kFragPostfilter_glsl.data()), kFragPostfilter_glsl.size()},
        .vs_post_filter =
            std::string_view{reinterpret_cast<const char *>(kVertPostfilter_glsl.data()), kVertPostfilter_glsl.size()},
        .target_width = 640,
        .target_height = 480,
    };

    try {
        text_shader_.emplace(
            gl::ShaderProgram{
                GetContext().GetExecutor(),
                std::string_view{reinterpret_cast<const char *>(kVertText_glsl.data()), kVertText_glsl.size()},
                std::string_view{reinterpret_cast<const char *>(kFragText_glsl.data()), kFragText_glsl.size()}});
    } catch (const std::exception &e) {
        wxLogError(wxString::Format("model viewer fatal error, cannot initialize text rendering: %s", e.what()));
        state_ = State::eGraphicsError;
        return;
    }

    RefreshText();

    try {
        device_ = std::make_unique<render::hal::RenderDeviceOpenGL40>(&GetContext(), hal_desc);
    } catch (const std::exception &e) {
        wxLogError(wxString::Format("model viewer fatal error, cannot initialize renderer device: %s", e.what()));
        state_ = State::eGraphicsError;
        return;
    }

    // std::vector<gmo::GmoModel> gmo_model;

    // try {
    //     GmoWxLogger logger;
    //     gmo_model = gmo::LoadModelFromMemory(gmo_buffer_, &logger);
    // } catch (const std::exception &e) {
    //     wxLogError(wxString::Format("libgmo fatal error: %s", e.what()));
    //     state_ = State::eInvalidModel;
    //     return;
    // }

    // wxLogMessage(wxString::Format("loaded %lld models from file", gmo_model.size()));
    std::fstream fs{"wakamo.act", std::ios::binary | std::ios::in};
    if (!fs.good()) {
        state_ = State::eGraphicsError;
        return;
    }

    fs.seekg(0, std::ios::end);
    const auto fsize = fs.tellg();
    fs.seekg(0, std::ios::beg);

    std::vector<uint8_t> buf;
    buf.resize(fsize);
    fs.read(reinterpret_cast<char *>(buf.data()), fsize);

    try {
        const auto act_model = act::LoadFromBinary(buf);
        model_ = conv::ConvertACT(act_model, device_.get());
        controller_ = model_->CreateController(model_->MakeAnimationList()[4]);
    } catch (const std::exception &e) {
        wxLogError(wxString::Format("model viewer fatal error, cannot convert model: %s", e.what()));
        state_ = State::eGraphicsError;
        return;
    }

    camera_.SetDistance(4.0f);

    animation_timer_->Begin();
    last_frame_ = std::chrono::steady_clock::now();
}

auto ModelViewer::OnRender() -> void {
    GL_IMPLEMENTATION_INTERNAL;

    if (!device_) {
        return;
    }

    if (pending_resize_.has_value()) {
        device_->ResizeFrame(pending_resize_->x, pending_resize_->y);
        pending_resize_.reset();
    }

    const auto now = std::chrono::steady_clock::now();
    const auto delta_time =
        static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(now - last_frame_).count()) * 10e-7f;
    last_frame_ = now;

    frame_timer_ += delta_time;
    if (frame_timer_ >= 1.0f) {
        frame_timer_ -= 1.0f;
        fps_ = frame_counter_;
        frame_counter_ = 1;

        RefreshText();
    } else {
        frame_counter_++;
    }

    controller_->Integrate(delta_time);

    const auto current_size = GetSize();
    const auto f_sw = static_cast<float>(current_size.x);
    const auto f_sh = static_cast<float>(current_size.y);

    GL_CHECK(glViewport(0, 0, current_size.x, current_size.y));
    GL_CHECK(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

    camera_.SetAspect(f_sw / f_sh);
    camera_.SetDistance(zoom_);
    camera_.SetCenter({0.0f, 0.5f, 0.0f});

    glm::fmat4x4 transform = glm::fmat4x4{1.0f};
    if (model_ && controller_.has_value()) {
        model_->Render(controller_->GetPose(), transform);
    }

    device_->RenderFrame(camera_);

    // clang-format off
    const glm::fmat4x4 text_matrix = {
        2.0f / f_sw, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / f_sh, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f,
    };
    // clang-format on

    GL_CHECK(glEnable(GL_BLEND));
    GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    if (!test_text_.has_value()) {
        return;
    }

    text_shader_->Use([&](const gl::ShaderProgram::Context &context) {
        gl::Texture::Use({gl::Texture::Binding{0, test_text_->GetTexture()}}, [&]() {
            context.SetUniform("u_view_projection", text_matrix);
            context.SetUniform("u_texture", 0);
            test_text_->GetMesh().Draw();
        });
    });
}

auto ModelViewer::OnSize(wxSizeEvent &event) -> void { pending_resize_ = {event.GetSize().x, event.GetSize().y}; }
auto ModelViewer::OnIdle([[maybe_unused]] wxIdleEvent &event) -> void { Render(); }

auto ModelViewer::OnMouseScroll([[maybe_unused]] wxMouseEvent &event) -> void {
    auto delta = event.GetWheelRotation() / event.GetWheelDelta();
    zoom_ = glm::clamp(zoom_ - (delta) * 0.05f, 0.2f, 10.0f);
}

auto ModelViewer::OnMouseMotion(wxMouseEvent &event) -> void {
    glm::ivec2 current_mouse_position;
    event.GetPosition(&current_mouse_position.x, &current_mouse_position.y);

    if (event.Dragging() && event.LeftIsDown()) {
        if (!prev_mouse_pos_.has_value()) {
            prev_mouse_pos_ = current_mouse_position;
            return;
        }

        const auto delta_position = current_mouse_position - prev_mouse_pos_.value();
        prev_mouse_pos_ = current_mouse_position;

        const auto delta_yaw = static_cast<float>(delta_position.x) / 300.0f;
        const auto delta_pitch = static_cast<float>(delta_position.y) / 300.0f;

        camera_.SetElevation(camera_.GetElevation() - delta_pitch);
        camera_.SetAzimuth(camera_.GetAzimuth() - delta_yaw);
    } else if (event.Dragging() && event.MiddleIsDown()) {

    } else {
        prev_mouse_pos_ = std::nullopt;
    }
}

auto ModelViewer::RefreshText() -> void {
    std::u32string content = fmt::format(U"fps: {}\nanimation: {}", fps_, anim_counter_);

    try {
        test_text_.emplace(font_context_.CreateTextObject(GetContext().GetExecutor(), {10.0f, 10.0f}, 32.0f, content));
    } catch (const std::exception &e) {
        wxLogError(wxString::Format("cannot update text: %s", e.what()));
    }
}
