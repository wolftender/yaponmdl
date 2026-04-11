#include <fstream>

#include "formats/gmo.hpp"
#include "formats/act.hpp"

#include "modelview.hpp"
#include "modelconv.hpp"

#include "generated/frag_mesh.glsl.h"
#include "generated/vert_skinmesh.glsl.h"
#include "generated/vert_staticmesh.glsl.h"

class GmoWxLogger : public gmo::GmoLogger {
public:
    auto log(std::string_view message) const -> void override {
        wxLogMessage("libgmo message: %s ", wxString{message.data(), message.size()});
    }
};

ModelViewer::ModelViewer(wxWindow *parent, const wxGLAttributes &attributes, std::span<const uint8_t> gmo_buffer)
    : GLView{parent, attributes}, gmo_buffer_{gmo_buffer} {

    Bind(wxEVT_IDLE, &ModelViewer::OnIdle, this);
    Bind(wxEVT_MOUSEWHEEL, &ModelViewer::OnMouseScroll, this);
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
    };

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
        controller_ = model_->CreateController(model_->MakeAnimationList().front());
    } catch (const std::exception &e) {
        wxLogError(wxString::Format("model viewer fatal error, cannot convert model: %s", e.what()));
        state_ = State::eGraphicsError;
        return;
    }

    camera_.SetPosition({0.0f, 0.0f, 4.0f});
}

auto ModelViewer::OnRender() -> void {
    GL_IMPLEMENTATION_INTERNAL;

    const auto current_size = GetSize();
    GL_CHECK(glViewport(0, 0, current_size.x, current_size.y));
    GL_CHECK(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));

    camera_.SetAspect(static_cast<float>(current_size.x) / static_cast<float>(current_size.y));

    if (model_ && controller_.has_value()) {
        model_->Render(controller_->GetPose(), glm::fmat4x4{1.0f});
    }

    device_->RenderFrame(camera_);
}

auto ModelViewer::OnIdle([[maybe_unused]] wxIdleEvent &event) -> void { Render(); }
auto ModelViewer::OnMouseScroll([[maybe_unused]] wxMouseEvent &event) -> void {}