#include <string_view>

#include "hexview.hpp"

HexViewer::HexViewer(
    wxWindow *parent, wxWindowID id, const wxPoint &position, const wxSize &size,
    std::optional<std::span<const uint8_t>> buffer)
    : wxWindow{parent, id, position, size}, buffer_view_{buffer} {
    text_control_ = new wxStyledTextCtrl(this, wxID_ANY);

    text_control_->SetMarginWidth(0, 0);
    text_control_->SetMarginWidth(1, 0);
    text_control_->SetMarginWidth(2, 0);

    text_control_->StyleSetFaceName(wxSTC_STYLE_DEFAULT, "Courier New");
    text_control_->StyleSetSize(wxSTC_STYLE_DEFAULT, 10);

    text_control_->StyleSetFaceName(wxSTC_STYLE_DEFAULT + 1, "Courier New");
    text_control_->StyleSetSize(wxSTC_STYLE_DEFAULT + 1, 10);
    text_control_->StyleSetForeground(wxSTC_STYLE_DEFAULT + 1, wxColor(0, 0, 128));
    text_control_->StyleSetBackground(wxSTC_STYLE_DEFAULT + 1, wxColor(255, 255, 255));
    text_control_->SetScrollWidth(0);
    text_control_->SetScrollWidthTracking(false);
    text_control_->SetUseHorizontalScrollBar(false);
    text_control_->SetUseVerticalScrollBar(false);

    scroll_bar_ = new wxScrollBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL);

    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(text_control_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM);
    sizer->Add(scroll_bar_, 0, wxEXPAND);

    text_control_->Bind(wxEVT_SIZE, &HexViewer::OnTextControlSize, this);
    text_control_->Bind(wxEVT_MOUSEWHEEL, &HexViewer::OnTextMouseScroll, this);
    scroll_bar_->Bind(wxEVT_SCROLL_CHANGED, &HexViewer::OnScroll, this);
    scroll_bar_->Bind(wxEVT_SCROLL_THUMBTRACK, &HexViewer::OnScroll, this);

    SetSizer(sizer);
    Layout();
}

auto HexViewer::SetBufferView(std::span<const uint8_t> buffer_view) -> void {
    if (buffer_view.size_bytes() > 0xffffffff) {
        throw std::runtime_error{"cannot display buffer, buffer is too big"};
    }

    buffer_view_ = buffer_view;
    UpdateTextControl(true);
}

auto HexViewer::ClearBufferView() -> void {
    buffer_view_ = std::nullopt;
    UpdateTextControl(true);
}

template <typename T> inline auto PrintAsHexToBuffer(const T value, std::vector<char>::iterator &out_buffer) -> void {
    constexpr std::string_view kHexDigits = "0123456789ABCDEF";
    constexpr auto kBitsInType = sizeof(T) * 8;

    for (uint32_t shift = kBitsInType - 4, i = 0; i < kBitsInType / 4; shift = shift - 4, ++i) {
        *out_buffer++ = kHexDigits[(value >> shift) & 0xf];
    }
}

auto HexViewer::OnScroll(wxScrollEvent &event) -> void {
    current_offset_ = scroll_bar_->GetThumbPosition();
    UpdateTextControl(false);
    event.Skip();
}

auto HexViewer::OnTextMouseScroll(wxMouseEvent &event) -> void {
    auto delta = event.GetWheelRotation() / event.GetWheelDelta();
    auto lines = delta * event.GetLinesPerAction();

    int64_t new_offset = static_cast<int64_t>(current_offset_) - lines;
    if (new_offset < 0) {
        current_offset_ = 0;
    } else {
        current_offset_ = static_cast<uint64_t>(new_offset);
    }

    UpdateTextControl(true);
}

auto HexViewer::OnTextControlSize([[maybe_unused]] wxSizeEvent &event) -> void { UpdateTextControl(true); }

/*
 * line looks like this
 *
 *  00000000: 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF  ................
 */

constexpr uint64_t kCharsPerAddress = 8;
constexpr uint64_t kHexPadCharacters = 2;
constexpr uint64_t kHexAreaCharacters = 16 * 3;
constexpr uint64_t kAsciiPadCharacters = 1;
constexpr uint64_t kAsciiAreaChatacters = 16;
constexpr uint64_t kTotalHexColCharacters =
    kCharsPerAddress + kHexPadCharacters + kHexAreaCharacters + kAsciiPadCharacters;
constexpr uint64_t kTotalLineCharacters = kTotalHexColCharacters + kAsciiAreaChatacters;
constexpr uint64_t kTotalLineCharactersPlusNewline = kTotalLineCharacters + 1;

auto HexViewer::UpdateTextControl(bool updated_size) -> void {
    if (!buffer_view_.has_value()) {
        return;
    }

    const auto buffer_view = buffer_view_.value();
    const uint64_t num_max_lines = buffer_view.size_bytes() / kBufferStride;
    const auto display_height = text_control_->LinesOnScreen();

    const uint64_t num_total_scroll_lines =
        num_max_lines + std::min(0ull, buffer_view.size_bytes() % kBufferStride) + 5;
    if (updated_size) {
        if (num_total_scroll_lines <= static_cast<uint32_t>(display_height)) {
            if (scroll_bar_->IsShown()) {
                scroll_bar_->Hide();
                Layout();
            }
        } else {
            if (!scroll_bar_->IsShown()) {
                scroll_bar_->Show(true);
                Layout();
            }

            scroll_bar_->SetScrollbar(current_offset_, display_height, num_total_scroll_lines, display_height);
        }
    }

    current_offset_ = std::min(num_max_lines, current_offset_);

    const auto buffer_size = buffer_view.size_bytes();
    const auto max_display_bytes = display_height * kBufferStride;
    const auto start_offset = current_offset_ * kBufferStride;
    const auto end_offset = std::min(start_offset + max_display_bytes, buffer_size);

    const auto num_lines = (end_offset - start_offset) / kBufferStride;
    const auto additional_bytes = (end_offset - start_offset) % kBufferStride;

    const uint64_t total_characters = kTotalHexColCharacters + num_lines * kTotalLineCharactersPlusNewline +
                                      ((additional_bytes > 0) ? kTotalHexColCharacters + additional_bytes : 0);

    std::vector<char> text_buffer;
    text_buffer.resize(total_characters);

    auto text_ptr = text_buffer.begin();

    // print the header line
    for (uint32_t i = 0; i < kCharsPerAddress + kHexPadCharacters; ++i) {
        *text_ptr++ = ' ';
    }

    for (uint8_t offset = 0; offset < kBufferStride; ++offset) {
        PrintAsHexToBuffer(offset, text_ptr);
        *text_ptr++ = ' ';
    }

    *text_ptr++ = '\n';

    // print hex body
    for (uint64_t line = 0; line < num_lines; ++line) {
        const uint32_t line_byte_offset = start_offset + (line * kBufferStride);
        PrintAsHexToBuffer(line_byte_offset, text_ptr);

        *text_ptr++ = ':';
        *text_ptr++ = ' ';

        for (uint64_t offs = line_byte_offset; offs < line_byte_offset + kBufferStride; ++offs) {
            PrintAsHexToBuffer(buffer_view[offs], text_ptr);
            *text_ptr++ = ' ';
        }

        *text_ptr++ = ' ';
        for (uint64_t offs = line_byte_offset; offs < line_byte_offset + kBufferStride; ++offs) {
            const uint8_t byte = buffer_view[offs];
            *text_ptr++ = (byte >= ' ' && byte <= '~') ? static_cast<char>(byte) : '.';
        }

        *text_ptr++ = '\n';
    }

    if (additional_bytes > 0) {
        const uint32_t last_line_offset = start_offset + (num_lines * kBufferStride);
        PrintAsHexToBuffer(last_line_offset, text_ptr);

        *text_ptr++ = ':';
        *text_ptr++ = ' ';

        for (uint64_t offs = last_line_offset; offs < last_line_offset + additional_bytes; ++offs) {
            PrintAsHexToBuffer(buffer_view[offs], text_ptr);
            *text_ptr++ = ' ';
        }

        for (uint64_t i = 0; i < (kBufferStride - additional_bytes) * 3; ++i) {
            *text_ptr++ = ' ';
        }

        *text_ptr++ = ' ';
        for (uint64_t offs = last_line_offset; offs < last_line_offset + additional_bytes; ++offs) {
            const uint8_t byte = buffer_view[offs];
            *text_ptr++ = (byte >= ' ' && byte <= '~') ? static_cast<char>(byte) : '.';
        }
    }

    text_control_->SetReadOnly(false);
    text_control_->ClearAll();
    text_control_->AddTextRaw(text_buffer.data(), text_buffer.size());

    // apply colors
    const auto total_lines = num_lines + ((additional_bytes > 0) ? 1 : 0);
    text_control_->StartStyling(0);
    text_control_->SetStyling(kTotalHexColCharacters, wxSTC_STYLE_DEFAULT + 1);

    for (uint64_t line = 0; line < total_lines; ++line) {
        text_control_->StartStyling(kTotalHexColCharacters + line * kTotalLineCharactersPlusNewline);
        text_control_->SetStyling(kCharsPerAddress, wxSTC_STYLE_DEFAULT + 1);
    }

    text_control_->SetReadOnly(true);
}
