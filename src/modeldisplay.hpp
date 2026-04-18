#pragma once
#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/dataview.h>

#include "modelview.hpp"

class ModelDisplay : public wxWindow {
public:
    ModelDisplay(
        std::span<const uint8_t> model_buffer, wxWindow *parent = nullptr, wxWindowID id = wxID_ANY,
        const wxPoint &position = wxDefaultPosition, const wxSize &size = wxDefaultSize);

private:
    auto BuildModelControls() -> void;
    auto OnModelLoaded(wxCommandEvent &event) -> void;
    auto OnSelectAnimation(wxDataViewEvent &event) -> void;

    wxSplitterWindow *splitter_ = nullptr;

    wxPanel *controls_ = nullptr;
    ModelViewer *viewer_ = nullptr;
};
