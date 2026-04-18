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

private:
    auto BuildModelControls() -> void;
    auto OnModelLoaded(wxCommandEvent &event) -> void;
    auto OnSelectAnimation(wxDataViewEvent &event) -> void;

    auto BuildAnimationLayout(const std::vector<std::string> &animations) -> void;
    auto BuildStaticLayout() -> void;

    wxBoxSizer *sizer_ = nullptr;
    wxSplitterWindow *splitter_ = nullptr;

    wxPanel *controls_ = nullptr;
    ModelViewer *viewer_ = nullptr;
};
