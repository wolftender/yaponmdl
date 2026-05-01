#pragma once
#include <wx/wx.h>
#include <wx/splitter.h>

class ConstrainedSplitter : public wxSplitterWindow {
public:
    ConstrainedSplitter(
        wxWindow *parent = nullptr, wxWindowID id = wxID_ANY, const wxPoint &position = wxDefaultPosition,
        const wxSize &size = wxDefaultSize, long style = wxSP_3D);

    auto GetMinimumWindow1Size() const -> uint32_t { return minimum_window_size_[0]; }
    auto GetMinimumWindow2Size() const -> uint32_t { return minimum_window_size_[1]; }
    auto SetMinimumWindow1Size(uint32_t minimum_size) -> void { minimum_window_size_[0] = minimum_size; }
    auto SetMinimumWindow2Size(uint32_t minimum_size) -> void { minimum_window_size_[1] = minimum_size; }

private:
    auto OnSize(wxSizeEvent &event) -> void;
    auto OnSashPosition(wxSplitterEvent &event) -> void;
    auto OnSashPosResize(wxSplitterEvent &event) -> void;

    auto AdjustSashPositonIfNeeded() const -> std::optional<int32_t>;

    uint32_t minimum_window_size_[2] = {0, 0};
};
