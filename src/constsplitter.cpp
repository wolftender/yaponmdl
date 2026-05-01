#include "constsplitter.hpp"

ConstrainedSplitter::ConstrainedSplitter(
    wxWindow *parent, wxWindowID id, const wxPoint &position, const wxSize &size, long style)
    : wxSplitterWindow(parent, id, position, size, style) {
    Bind(wxEVT_SIZE, &ConstrainedSplitter::OnSize, this);
    Bind(wxEVT_SPLITTER_SASH_POS_RESIZE, &ConstrainedSplitter::OnSashPosResize, this);
    Bind(wxEVT_SPLITTER_SASH_POS_CHANGING, &ConstrainedSplitter::OnSashPosition, this);
}

auto ConstrainedSplitter::OnSize([[maybe_unused]] wxSizeEvent &event) -> void {
    const auto new_sash_pos = AdjustSashPositonIfNeeded();
    if (new_sash_pos.has_value()) {
        SetSashPosition(*new_sash_pos);
    }

    event.Skip();
}

auto ConstrainedSplitter::OnSashPosition(wxSplitterEvent &event) -> void {
    const auto size = GetSplitMode() == wxSPLIT_VERTICAL ? GetSize().x : GetSize().y;
    const auto sash_position = event.GetSashPosition();

    const auto win1_min_size = static_cast<int32_t>(minimum_window_size_[0]);
    const auto win2_min_size = static_cast<int32_t>(minimum_window_size_[1]);

    const auto new_win1_size = sash_position;
    const auto new_win2_size = size - sash_position;

    if (new_win1_size < win1_min_size || new_win2_size < win2_min_size) {
        event.Veto();
    } else {
        event.Skip();
    }
}

auto ConstrainedSplitter::OnSashPosResize(wxSplitterEvent &event) -> void {
    const auto new_sash_pos = AdjustSashPositonIfNeeded();
    if (new_sash_pos.has_value()) {
        event.SetSashPosition(*new_sash_pos);
    } else {
        event.Skip();
    }
}

auto ConstrainedSplitter::AdjustSashPositonIfNeeded() const -> std::optional<int32_t> {
    const auto size = GetSplitMode() == wxSPLIT_VERTICAL ? GetSize().x : GetSize().y;
    auto sash_position = GetSashPosition();

    const auto win1_min_size = static_cast<int32_t>(minimum_window_size_[0]);
    const auto win2_min_size = static_cast<int32_t>(minimum_window_size_[1]);

    const auto new_win1_size = sash_position;
    const auto new_win2_size = size - sash_position;

    if (new_win1_size < win1_min_size) {
        return win1_min_size;
    } else if (new_win2_size < win2_min_size) {
        return size - win2_min_size;
    }

    return std::nullopt;
}
