#include <fmt/format.h>

#include "modelview.hpp"
#include "resourcestore.hpp"

wxDEFINE_EVENT(MODEL_VIEWER_LOADED_MODEL, wxCommandEvent);

ModelViewer::AzimuthCameraController::AzimuthCameraController() {
    camera_.SetNear(0.1f);
    camera_.SetFar(300.0f);
    camera_.SetDistance(zoom_);
    camera_.SetCenter({0.0f, 0.5f, 0.0f});
}

auto ModelViewer::AzimuthCameraController::OnUpdateSize(float width, float height) -> void {
    camera_.SetAspect(width / height);
}

auto ModelViewer::AzimuthCameraController::OnMouseMotion(wxMouseEvent &event) -> void {
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

auto ModelViewer::AzimuthCameraController::OnMouseScroll(wxMouseEvent &event) -> void {
    auto delta = event.GetWheelRotation() / event.GetWheelDelta();
    zoom_ = glm::clamp(zoom_ - (delta) * 0.05f, 0.2f, 10.0f);
    camera_.SetDistance(zoom_);
}

ModelViewer::OrthoCameraController::OrthoCameraController() {
    camera_.SetNear(-10.0f);
    camera_.SetFar(10.0f);
    camera_.SetFar(100.0f);
}

auto ModelViewer::OrthoCameraController::OnUpdateSize(float width, float height) -> void {
    size_ = {width, height};
    camera_.SetSize(size_ * zoom_);
}

auto ModelViewer::OrthoCameraController::OnMouseMotion([[maybe_unused]] wxMouseEvent &event) -> void {}

auto ModelViewer::OrthoCameraController::OnMouseScroll([[maybe_unused]] wxMouseEvent &event) -> void {
    auto delta = event.GetWheelRotation() / event.GetWheelDelta();
    zoom_ = glm::clamp(zoom_ - (delta) * 0.05f, 0.2f, 10.0f);
    camera_.SetSize(size_ * zoom_);
}

ModelViewer::ModelViewer(
    wxWindow *parent, const wxGLAttributes &attributes, std::unique_ptr<ILoader> loader,
    std::unique_ptr<ICameraController> camera_controller)
    : GLView{parent, attributes}, model_loader_{std::move(loader)}, camera_controller_{std::move(camera_controller)},
      font_context_{memoryresource::GetFontOpensansTtf()} {

    animation_timer_ = std::make_unique<AnimationTimer>(this);

    Bind(wxEVT_SIZE, &ModelViewer::OnSize, this);
    Bind(wxEVT_IDLE, &ModelViewer::OnIdle, this);
    Bind(wxEVT_MOUSEWHEEL, &ModelViewer::OnMouseScroll, this);
    Bind(wxEVT_MOTION, &ModelViewer::OnMouseMotion, this);
}

auto ModelViewer::GetAnimationList() const -> std::vector<std::string> {
    if (!model_) {
        return {};
    }

    std::vector<std::string> animation_list;
    const auto animations = model_->MakeAnimationList();

    animation_list.reserve(animations.size());

    for (const auto &animation_id : animations) {
        const auto *animation = model_->GetAnimation(animation_id);
        if (!animation) {
            wxLogError("model assertion failed for animation list");
            return {};
        }

        animation_list.push_back(animation->GetName());
    }

    return animation_list;
}

auto ModelViewer::SetAnimationIndex(uint32_t index) -> void {
    const auto animations = model_->MakeAnimationList();
    if (index >= animations.size()) {
        anim_counter_ = animations.size() - 1;
    } else {
        anim_counter_ = index;
    }

    if (controller_) {
        controller_->SetAnimation(animations[anim_counter_]);
    }

    RefreshText();
}

auto ModelViewer::OnInitializeGL() -> void {
    GL_IMPLEMENTATION_INTERNAL;

    render::hal::RenderDeviceOpenGL40::Description hal_desc = {
        .fs_static_source = memoryresource::GetFragShaderMesh(),
        .vs_static_source = memoryresource::GetVertShaderStaticMesh(),
        .fs_skinned_source = memoryresource::GetFragShaderMesh(),
        .vs_skinned_source = memoryresource::GetVertShaderSkinnedMesh(),
        .fs_post_filter = memoryresource::GetFragShaderPost(),
        .vs_post_filter = memoryresource::GetVertShaderPost(),
        .target_width = 640,
        .target_height = 480,
    };

    try {
        text_shader_.emplace(
            gl::ShaderProgram{
                GetContext().GetExecutor(), memoryresource::GetVertShaderText(), memoryresource::GetFragShaderText()});
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

    if (!model_loader_) {
        wxLogError("invalid model loader specified");
        state_ = State::eInvalidModel;
        return;
    }

    model_ = model_loader_->Load(*device_);

    if (!model_) {
        wxLogError("no models were loaded");
        state_ = State::eInvalidModel;
        return;
    }

    try {
        const auto animations = model_->MakeAnimationList();
        if (animations.empty()) {
            default_pose_ = model_->CreatePose();
        } else {
            controller_ = model_->CreateController(model_->MakeAnimationList().front());
        }
    } catch (const std::exception &e) {
        wxLogError(wxString::Format("cannot initialize model controller: %s", e.what()));
        state_ = State::eInvalidModel;
        return;
    }

    animation_timer_->Begin();
    last_frame_ = std::chrono::steady_clock::now();

    wxCommandEvent event{MODEL_VIEWER_LOADED_MODEL, GetId()};
    ProcessEvent(event);

    RefreshText();
    state_ = State::eReady;
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

    if (state_ != State::eReady) {
        return;
    }

    if (controller_.has_value()) {
        controller_->Integrate(delta_time);
    }

    const auto current_size = GetSize();
    const auto f_sw = static_cast<float>(current_size.x);
    const auto f_sh = static_cast<float>(current_size.y);

    GL_CHECK(glViewport(0, 0, current_size.x, current_size.y));
    GL_CHECK(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

    camera_controller_->OnUpdateSize(f_sw, f_sh);

    glm::fmat4x4 transform = glm::fmat4x4{1.0f};

    if (model_ && controller_.has_value()) {
        model_->Render(controller_->GetPose(), transform);
    } else if (model_ && default_pose_) {
        model_->Render(*default_pose_.get(), transform);
    }

    device_->RenderFrame(camera_controller_->GetCamera());

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

auto ModelViewer::OnSize(wxSizeEvent &event) -> void {
    event.Skip();
    pending_resize_ = {event.GetSize().x, event.GetSize().y};
}

auto ModelViewer::OnIdle([[maybe_unused]] wxIdleEvent &event) -> void { Render(); }

auto ModelViewer::OnMouseScroll([[maybe_unused]] wxMouseEvent &event) -> void {
    camera_controller_->OnMouseScroll(event);
}

auto ModelViewer::OnMouseMotion(wxMouseEvent &event) -> void {
    event.Skip();
    camera_controller_->OnMouseMotion(event);
}

auto ModelViewer::RefreshText() -> void {
    const auto animations = model_ ? model_->MakeAnimationList() : std::vector<render::Model::AnimationId>{};
    std::string content;

    if (animations.size() != 0 && anim_counter_ < animations.size()) {
        content =
            fmt::format("fps: {}\nanimation: {}", fps_, model_->GetAnimation(animations[anim_counter_])->GetName());
    } else {
        content = fmt::format("fps: {}\nanimation: n/a", fps_);
    }

    std::u32string codepoints;
    const wxString wxstr{content};

    codepoints.reserve(wxstr.Length());
    for (const wxUniChar unicode_char : wxstr) {
        codepoints.push_back(unicode_char.GetValue());
    }

    try {
        test_text_.emplace(
            font_context_.CreateTextObject(GetContext().GetExecutor(), {10.0f, 10.0f}, 32.0f, codepoints));
    } catch (const std::exception &e) {
        wxLogError(wxString::Format("cannot update text: %s", e.what()));
    }
}
