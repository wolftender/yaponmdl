#pragma once
#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/dataview.h>

#include "modelview.hpp"

class ModelDisplay : public wxWindow {
public:
    ModelDisplay(
        std::unique_ptr<ModelViewer::ILoader> loader,
        std::unique_ptr<ModelViewer::ICameraController> camera_controller = ModelViewer::MakeAzimuthCamera(),
        wxWindow *parent = nullptr, wxWindowID id = wxID_ANY, const wxPoint &position = wxDefaultPosition,
        const wxSize &size = wxDefaultSize);

    auto GetViewer() const -> const ModelViewer * { return viewer_; }
    auto GetViewer() -> ModelViewer * { return viewer_; }

private:
    auto BuildModelControls() -> void;
    auto OnModelLoaded(wxCommandEvent &event) -> void;
    auto OnFrameRendered(ModelViewerFrameEvent &event) -> void;
    auto OnSelectAnimation(wxDataViewEvent &event) -> void;

    auto OnPausePressed(wxCommandEvent &event) -> void;
    auto OnPlayPressed(wxCommandEvent &event) -> void;
    auto OnSliderUpdated(wxCommandEvent &event) -> void;

    auto BuildAnimationLayout(std::span<const std::string> animations) -> void;
    auto BuildStaticLayout() -> void;

    wxBoxSizer *sizer_ = nullptr;
    wxSplitterWindow *splitter_ = nullptr;

    wxPanel *controls_ = nullptr;
    wxSlider *animation_slider_ = nullptr;
    ModelViewer *viewer_ = nullptr;
};
