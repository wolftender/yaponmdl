#include "modeldisplay.hpp"

ModelDisplay::ModelDisplay(
    std::span<const uint8_t> model_buffer, wxWindow *parent, wxWindowID id, const wxPoint &position, const wxSize &size)
    : wxWindow{parent, id, position, size} {
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);

    SetMinSize(wxSize{600, 200});
    sizer->SetMinSize(600, 200);

    splitter_ = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxSize{500, 500}, wxSP_3D | wxSP_LIVE_UPDATE);
    splitter_->SetSashGravity(1.0);
    splitter_->SetSashPosition(300);
    splitter_->SetMinimumPaneSize(250);

    viewer_ = new ModelViewer(splitter_, GLView::CreateAttributes(), model_buffer);

    // this callback has to be registered early because it will be called as soon as gl context is initialized
    viewer_->Bind(MODEL_VIEWER_LOADED_MODEL, &ModelDisplay::OnModelLoaded, this);
    controls_ = new wxPanel(splitter_, wxID_ANY);
    controls_->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

    const auto window_width = GetSize().x;
    const auto right_pane_width = window_width / 4;

    sizer->Add(splitter_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM);
    splitter_->SplitVertically(viewer_, controls_, -right_pane_width);

    SetSizerAndFit(sizer);
}

auto ModelDisplay::OnSelectAnimation(wxDataViewEvent &event) -> void {
    const auto item = event.GetItem();

    if (item.IsOk()) {
        auto *list = static_cast<wxDataViewListCtrl *>(event.GetEventObject());
        viewer_->SetAnimationIndex(list->ItemToRow(item));
    }
}

auto ModelDisplay::OnModelLoaded([[maybe_unused]] wxCommandEvent &event) -> void {
    controls_->DestroyChildren();
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer *anim_panel = new wxStaticBoxSizer(wxVERTICAL, controls_, "Animations");
    wxDataViewListCtrl *anim_list_box = new wxDataViewListCtrl(
        controls_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES | wxDV_NO_HEADER | wxDV_SINGLE);

    const auto animations = viewer_->GetAnimationList();
    anim_list_box->AppendTextColumn("Name");
    for (const auto &animation : animations) {
        anim_list_box->AppendItem({animation});
    }

    anim_list_box->SelectRow(viewer_->GetAnimationIndex());
    anim_panel->Add(anim_list_box, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM);
    sizer->Add(anim_panel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM);

    controls_->SetSizerAndFit(sizer);

    anim_list_box->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &ModelDisplay::OnSelectAnimation, this);
}
