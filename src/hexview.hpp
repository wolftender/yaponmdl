#pragma once
#include <span>

#include <wx/wx.h>
#include <wx/stc/stc.h>

class HexViewer final : public wxWindow {
public:
    static constexpr uint32_t kBufferStride = 16;

    HexViewer(
        wxWindow *parent = nullptr, wxWindowID id = wxID_ANY, const wxPoint &position = wxDefaultPosition,
        const wxSize &size = wxDefaultSize, std::optional<std::span<const uint8_t>> buffer_view = std::nullopt);

    auto SetBufferView(std::span<const uint8_t> buffer_view) -> void;
    auto ClearBufferView() -> void;

private:
    auto OnScroll(wxScrollEvent &event) -> void;
    auto OnTextMouseScroll(wxMouseEvent &event) -> void;
    auto OnTextControlSize(wxSizeEvent &event) -> void;
    auto UpdateTextControl(bool updated_size) -> void;

    wxStyledTextCtrl *text_control_ = nullptr;
    wxScrollBar *scroll_bar_ = nullptr;

    uint64_t current_offset_ = 0;
    std::optional<std::span<const uint8_t>> buffer_view_ = {};
};
