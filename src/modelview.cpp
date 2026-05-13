#include <fmt/format.h>

#include "modelview.hpp"
#include "resourcestore.hpp"

wxDEFINE_EVENT(MODEL_VIEWER_LOADED_MODEL, wxCommandEvent);
wxDEFINE_EVENT(MODEL_VIEWER_FRAME, ModelViewerFrameEvent);

ModelViewer::ModelProxyModel::ModelProxyModel(std::unique_ptr<render::Model> model) {
    model_ = std::move(model);
    bind_pose_ = model_->CreatePose();
    animations_ = model_->MakeAnimationList();

    if (!animations_.empty()) {
        controller_ = model_->CreateController(animations_.front());
    }

    // cache animation names
    animation_names_.reserve(animations_.size());

    for (const auto &animation_id : animations_) {
        const auto *animation = model_->GetAnimation(animation_id);
        if (!animation) {
            wxLogError("model assertion failed for animation list");
            continue;
        }

        animation_names_.push_back(animation->GetName());
    }
}

auto ModelViewer::ModelProxyModel::HasAnimations() const -> bool { return !animations_.empty(); }

auto ModelViewer::ModelProxyModel::GetLoop() const -> bool {
    if (controller_) {
        return controller_->GetLoop();
    }

    return false;
}

auto ModelViewer::ModelProxyModel::GetPaused() const -> bool { return paused_; }

auto ModelViewer::ModelProxyModel::GetAnimationDuration() const -> float {
    if (controller_) {
        return controller_->GetDuration();
    }

    return 0.0f;
}

auto ModelViewer::ModelProxyModel::GetAnimationTime() const -> float {
    if (controller_) {
        return controller_->GetTime();
    }

    return 0.0f;
}

auto ModelViewer::ModelProxyModel::GetAnimationList() const -> std::span<const std::string> { return animation_names_; }

auto ModelViewer::ModelProxyModel::SetPaused(bool paused) -> void { paused_ = paused; };

auto ModelViewer::ModelProxyModel::SetLoop(bool loop) -> void {
    if (controller_) {
        controller_->SetLoop(loop);
    }
}

auto ModelViewer::ModelProxyModel::Seek(float time) -> void {
    if (controller_) {
        const auto loop = controller_->GetLoop();

        controller_->SetLoop(false);
        controller_->Seek(time);
        controller_->SetLoop(loop);
    }
}

auto ModelViewer::ModelProxyModel::SetAnimationIndex(uint32_t index) -> void {
    if (controller_ && index < animations_.size()) {
        controller_->SetAnimation(animations_[index]);
        controller_->Integrate(0.0f);
    }
}

auto ModelViewer::ModelProxyModel::Integrate(float delta_time) -> void {
    if (controller_ && !paused_) {
        controller_->Integrate(delta_time);
    }
}

auto ModelViewer::ModelProxyModel::Render(const glm::fmat4x4 &world_matrix) -> void {
    if (controller_) {
        model_->Render(controller_->GetPose(), world_matrix);
    } else {
        model_->Render(*bind_pose_, world_matrix);
    }
}

ModelViewer::ModelProxyDrawlist::ModelProxyDrawlist(std::unique_ptr<render::Drawlist> drawlist) {
    drawlist_ = std::move(drawlist);
    animations_ = drawlist_->MakeMotionList();

    if (animations_.empty()) {
        throw std::runtime_error{"selected drawlist is not renderable because it has no frames to render"};
    }

    controller_ = drawlist_->CreateController(animations_.front());

    // cache animation names
    animation_names_.reserve(animations_.size());

    for (const auto &animation_id : animations_) {
        const auto *animation = drawlist_->GetMotion(animation_id);
        if (!animation) {
            wxLogError("model assertion failed for animation list");
            continue;
        }

        animation_names_.push_back(std::string{animation->GetName()});
    }
}

auto ModelViewer::ModelProxyDrawlist::HasAnimations() const -> bool { return !animations_.empty(); }
auto ModelViewer::ModelProxyDrawlist::GetLoop() const -> bool { return controller_->GetLoop(); }
auto ModelViewer::ModelProxyDrawlist::GetPaused() const -> bool { return paused_; }
auto ModelViewer::ModelProxyDrawlist::GetAnimationDuration() const -> float { return controller_->GetDuration(); }
auto ModelViewer::ModelProxyDrawlist::GetAnimationTime() const -> float { return controller_->GetTime(); }

auto ModelViewer::ModelProxyDrawlist::GetAnimationList() const -> std::span<const std::string> {
    return animation_names_;
}

auto ModelViewer::ModelProxyDrawlist::SetPaused(bool paused) -> void { paused_ = paused; };
auto ModelViewer::ModelProxyDrawlist::SetLoop(bool loop) -> void { controller_->SetLoop(loop); }

auto ModelViewer::ModelProxyDrawlist::Seek(float time) -> void {
    const auto loop = controller_->GetLoop();

    controller_->SetLoop(false);
    controller_->Seek(time);
    controller_->SetLoop(loop);
}

auto ModelViewer::ModelProxyDrawlist::SetAnimationIndex(uint32_t index) -> void {
    if (index < animations_.size()) {
        controller_->SetMotion(animations_[index]);
        controller_->Integrate(0.0f);
    }
}

auto ModelViewer::ModelProxyDrawlist::Integrate(float delta_time) -> void {
    if (!paused_) {
        controller_->Integrate(delta_time);
    }
}

auto ModelViewer::ModelProxyDrawlist::Render(const glm::fmat4x4 &world_matrix) -> void {
    controller_->Render(world_matrix);
}

ModelViewer::AzimuthCameraController::AzimuthCameraController() {
    camera_.SetNear(0.1f);
    camera_.SetFar(300.0f);
    camera_.SetDistance(zoom_);
    camera_.SetCenter({0.0f, 0.5f, 0.0f});
}

auto ModelViewer::AzimuthCameraController::ZoomOut() -> void {
    zoom_ = zoom_ + 0.25f;
    SetCameraParameters();
}

auto ModelViewer::AzimuthCameraController::ZoomIn() -> void {
    zoom_ = zoom_ - 0.25f;
    SetCameraParameters();
}

auto ModelViewer::AzimuthCameraController::ResetView() -> void {
    zoom_ = 1.0f;
    camera_.SetNear(0.1f);
    camera_.SetFar(300.0f);
    camera_.SetDistance(zoom_);
    camera_.SetCenter({0.0f, 0.5f, 0.0f});
    camera_.SetAzimuth(0.0f);
    camera_.SetElevation(0.0f);
    SetCameraParameters();
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

    SetCameraParameters();
}

auto ModelViewer::AzimuthCameraController::OnMouseScroll(wxMouseEvent &event) -> void {
    auto delta = event.GetWheelRotation() / event.GetWheelDelta();
    zoom_ = zoom_ - (delta * 0.05f);
    SetCameraParameters();
}

auto ModelViewer::AzimuthCameraController::SetCameraParameters() -> void {
    zoom_ = glm::clamp(zoom_, 0.2f, 10.0f);
    camera_.SetDistance(zoom_);
}

ModelViewer::OrthoCameraController::OrthoCameraController() : center_{0.0f, 0.0f} {
    camera_.SetNear(-1000.0f);
    camera_.SetFar(1000.0f);
}

auto ModelViewer::OrthoCameraController::ZoomOut() -> void {
    zoom_ = zoom_ + 0.25f;
    SetCameraParameters();
}

auto ModelViewer::OrthoCameraController::ZoomIn() -> void {
    zoom_ = zoom_ - 0.25f;
    SetCameraParameters();
}

auto ModelViewer::OrthoCameraController::ResetView() -> void {
    center_ = {0.0f, 0.0f};
    zoom_ = 1.0f;
    SetCameraParameters();
}

auto ModelViewer::OrthoCameraController::OnUpdateSize(float width, float height) -> void {
    size_ = {width, height};
    camera_.SetSize(size_ * zoom_);
}

auto ModelViewer::OrthoCameraController::OnMouseMotion([[maybe_unused]] wxMouseEvent &event) -> void {
    glm::ivec2 current_mouse_position;
    event.GetPosition(&current_mouse_position.x, &current_mouse_position.y);

    if (event.Dragging() && event.LeftIsDown()) {
        if (!prev_mouse_pos_.has_value()) {
            prev_mouse_pos_ = current_mouse_position;
            return;
        }

        const auto delta_position = current_mouse_position - prev_mouse_pos_.value();
        prev_mouse_pos_ = current_mouse_position;
        center_ =
            center_ + glm::fvec2{-static_cast<float>(delta_position.x), static_cast<float>(delta_position.y)} * zoom_;

        SetCameraParameters();
    } else {
        prev_mouse_pos_ = std::nullopt;
    }
}

auto ModelViewer::OrthoCameraController::OnMouseScroll([[maybe_unused]] wxMouseEvent &event) -> void {
    auto delta = event.GetWheelRotation() / event.GetWheelDelta();
    zoom_ = zoom_ - (delta * 0.05f);

    SetCameraParameters();
}

auto ModelViewer::OrthoCameraController::SetCameraParameters() -> void {
    zoom_ = glm::clamp(zoom_, 0.2f, 10.0f);
    camera_.SetSize(size_ * zoom_);
    camera_.SetCenter(glm::fvec3{center_, 0.0f});
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

auto ModelViewer::GetAnimationList() const -> std::span<const std::string> {
    if (model_proxy_) {
        return model_proxy_->GetAnimationList();
    }

    return {};
}

auto ModelViewer::SetAnimationIndex(uint32_t index) -> void {
    if (model_proxy_) {
        model_proxy_->SetAnimationIndex(index);
    }

    RefreshText();
}

auto ModelViewer::ZoomIn() -> void {
    if (camera_controller_) {
        camera_controller_->ZoomIn();
    }
}

auto ModelViewer::ZoomOut() -> void {
    if (camera_controller_) {
        camera_controller_->ZoomOut();
    }
}

auto ModelViewer::ResetView() -> void {
    if (camera_controller_) {
        camera_controller_->ResetView();
    }
}

auto ModelViewer::GetCurrentAnimationTime() const -> float {
    if (model_proxy_) {
        return model_proxy_->GetAnimationTime();
    }

    return 0.0f;
}

auto ModelViewer::GetCurrentAnimationDuration() const -> float {
    if (model_proxy_) {
        return model_proxy_->GetAnimationDuration();
    }

    return 0.0f;
}

auto ModelViewer::IsCurrentAnimationPaused() const -> bool {
    if (model_proxy_) {
        return model_proxy_->GetPaused();
    }

    return true;
}

auto ModelViewer::SetCurrentAnimationPaused(bool paused) -> void {
    if (model_proxy_) {
        model_proxy_->SetPaused(paused);
    }
}

auto ModelViewer::SeekCurrentAnimation(float time) -> void {
    if (model_proxy_) {
        model_proxy_->Seek(time);
    }
}

auto ModelViewer::OnInitializeGL() -> void {
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

    RefreshText();

    try {
        device_ = std::make_unique<render::hal::RenderDeviceOpenGL40>(
            &GetContext(), render::hal::RenderDeviceOpenGL40::RendererType::eBaseSceneRenderer, 640, 480);
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

    model_proxy_ = model_loader_->Load(*device_);

    if (!model_proxy_) {
        wxLogError("no models were loaded");
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

    model_proxy_->Integrate(delta_time);

    const auto current_size = GetSize();
    const auto f_sw = static_cast<float>(current_size.x);
    const auto f_sh = static_cast<float>(current_size.y);

    GL_CHECK(glViewport(0, 0, current_size.x, current_size.y));
    GL_CHECK(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

    camera_controller_->OnUpdateSize(f_sw, f_sh);

    glm::fmat4x4 transform = glm::fmat4x4{1.0f};

    model_proxy_->Render(transform);
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

    // finished rendering frame event
    ModelViewerFrameEvent event{MODEL_VIEWER_FRAME, GetId()};
    event.SetDeltaTime(delta_time);
    ProcessEvent(event);
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
    static std::vector<std::string> kEmptyAnimList;

    const auto animations = model_proxy_ ? model_proxy_->GetAnimationList() : kEmptyAnimList;
    std::string content;

    if (animations.size() != 0 && anim_counter_ < animations.size()) {
        content = fmt::format("fps: {}\nanimation: {}", fps_, animations[anim_counter_]);
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
